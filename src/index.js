export let handoff;

if (!handoff) {
  try {
    handoff = require('../build/Release/terminal-handoff.node');
  } catch (outerError) {
    try {
      handoff = require('../build/Debug/terminal-handoff.node');
    } catch (innerError) {
      console.error('innerError', innerError);
      // Re-throw the exception from the Release require if the Debug require fails as well
      throw outerError;
    }
  }
}
