#define _WIN32_WINNT 0x600
#define WIN32_LEAN_AND_MEAN

#include <napi.h>
#include <string>
#include <charconv>
#include <cassert>
#include <windows.h>
#include <wrl.h>
#include "ITerminalHandoff.h"

[[noreturn]] void ThrowHrMsg(napi_env env, HRESULT hr, const std::string_view msg) {
  constexpr std::string_view suffix = " (0x00000000)";

  std::string fullMsg;
  fullMsg.reserve(msg.size() + suffix.size());
  fullMsg.assign(msg);
  fullMsg.append(suffix);

  char* const first = fullMsg.data() + msg.size() + 4; // strlen(" (0x") = 4
  char* const last = first + 8; // strlen("00000000") = 8
  const auto result = std::to_chars(first, last, std::make_unsigned_t<HRESULT>(hr), 16);
  assert(result.ec == std::errc()); // no error
  assert(result.ptr == last); // 8 digits

  throw Napi::Error::New(env, fullMsg);
}

static std::mutex g_mutex;
static bool g_once = false;
static DWORD g_terminalHandoffRegistration = 0;
static Napi::ThreadSafeFunction g_callback;

void LockedUnregister() {
  if (g_terminalHandoffRegistration != 0) {
    CoRevokeClassObject(g_terminalHandoffRegistration);
    g_terminalHandoffRegistration = 0;

    CoReleaseServerProcess();
  }
}

class TerminalHandoff
  : public Microsoft::WRL::RuntimeClass<
  Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
  ITerminalHandoff2>
{
public:
  virtual HRESULT STDMETHODCALLTYPE EstablishPtyHandoff(
    /* [system_handle][in] */ HANDLE in,
    /* [system_handle][in] */ HANDLE out,
    /* [system_handle][in] */ HANDLE signal,
    /* [system_handle][in] */ HANDLE ref,
    /* [system_handle][in] */ HANDLE server,
    /* [system_handle][in] */ HANDLE client,
    /* [in] */ TERMINAL_STARTUP_INFO startupInfo) override
  {
    const std::lock_guard<std::mutex> lock(g_mutex);

    if (g_once) {
      LockedUnregister();
    }

    HANDLE hCurProcess = GetCurrentProcess();
    DuplicateHandle(hCurProcess, in, hCurProcess, &in, 0, FALSE, DUPLICATE_SAME_ACCESS);
    DuplicateHandle(hCurProcess, out, hCurProcess, &out, 0, FALSE, DUPLICATE_SAME_ACCESS);
    DuplicateHandle(hCurProcess, signal, hCurProcess, &signal, 0, FALSE, DUPLICATE_SAME_ACCESS);
    DuplicateHandle(hCurProcess, ref, hCurProcess, &ref, 0, FALSE, DUPLICATE_SAME_ACCESS);
    DuplicateHandle(hCurProcess, server, hCurProcess, &server, 0, FALSE, DUPLICATE_SAME_ACCESS);
    DuplicateHandle(hCurProcess, client, hCurProcess, &client, 0, FALSE, DUPLICATE_SAME_ACCESS);

    g_callback.BlockingCall([=](Napi::Env env, Napi::Function callback) {
      callback({
        Napi::Number::New(env, reinterpret_cast<size_t>(in)),
        Napi::Number::New(env, reinterpret_cast<size_t>(out)),
        Napi::Number::New(env, reinterpret_cast<size_t>(signal)),
        Napi::Number::New(env, reinterpret_cast<size_t>(ref)),
        Napi::Number::New(env, reinterpret_cast<size_t>(server)),
        Napi::Number::New(env, reinterpret_cast<size_t>(client)),
      });
    });

    if (g_once) {
      g_callback.Release();
      g_callback = {};
    }

    return S_OK;
  }
};

static Napi::Value Register(const Napi::CallbackInfo& info) {
  Napi::Env env(info.Env());
  Napi::HandleScope scope(env);

  if (info.Length() != 3 ||
    !info[0].IsString() ||
    !info[1].IsFunction() ||
    !info[2].IsBoolean()) {
    throw Napi::Error::New(env, "Usage: register(clsid, callback, once)");
  }

  const auto clsidStr = info[0].As<Napi::String>().Utf16Value();
  Napi::Function callback = info[1].As<Napi::Function>();
  const bool once = info[2].As<Napi::Boolean>();

  static_assert(sizeof(char16_t) == sizeof(OLECHAR));
  CLSID clsid;
  HRESULT hr = CLSIDFromString(reinterpret_cast<LPCOLESTR>(clsidStr.c_str()), &clsid);
  if (FAILED(hr)) {
    ThrowHrMsg(env, hr, "Failed to parse CLSID string");
  }

  do {
    const std::lock_guard<std::mutex> lock(g_mutex);

    if (g_terminalHandoffRegistration != 0) {
      break;
    }

    g_once = once;

    using namespace Microsoft::WRL;

    const auto classFactory = Make<SimpleClassFactory<TerminalHandoff>>();

    ComPtr<IUnknown> unk;
    hr = classFactory.As(&unk);
    if (FAILED(hr)) {
      break;
    }

    hr = CoRegisterClassObject(clsid, unk.Get(), CLSCTX_LOCAL_SERVER, once ? REGCLS_SINGLEUSE : REGCLS_MULTIPLEUSE, &g_terminalHandoffRegistration);
    if (FAILED(hr)) {
      break;
    }

    CoAddRefServerProcess();

    g_callback = Napi::ThreadSafeFunction::New(env, callback, "EstablishPtyHandoff Callback", 0, 1);
  } while (0);

  if (FAILED(hr)) {
    ThrowHrMsg(env, hr, "Failed to register class object");
  }

  return env.Undefined();
}

static Napi::Value Unregister(const Napi::CallbackInfo& info) {
  Napi::Env env(info.Env());
  Napi::HandleScope scope(env);

  const std::lock_guard<std::mutex> lock(g_mutex);

  if (g_terminalHandoffRegistration != 0) {
    g_callback.Release();
    g_callback = {};
  }

  LockedUnregister();

  return env.Undefined();
}

/**
* Init
*/

Napi::Object init(Napi::Env env, Napi::Object exports) {
  exports.Set("register", Napi::Function::New(env, Register));
  exports.Set("unregister", Napi::Function::New(env, Unregister));
  return exports;
};

NODE_API_MODULE(NODE_GYP_MODULE_NAME, init);
