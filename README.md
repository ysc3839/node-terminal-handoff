# node-terminal-handoff

```js
const pty = require('node-pty');
const handoff = require('terminal-handoff');

handoff.register(/*clsid=*/'{E12CFF52-A866-4C77-9A90-F570A7AA2C6B}', /*callback=*/function (input, output, signal, ref, server, client) {
  const handoffHandles = { input, output, signal, ref, server, client };
  const pty = pty.handoff(handoffHandles);
}, /*once=*/false);
```
