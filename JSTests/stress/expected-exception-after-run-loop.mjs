//@ requireOptions("--exception=TypeError")

// The expected exception is only thrown once a timer has fired, so the shell has to run its run
// loop before it can check the module's rejection against --exception.
asyncTestStart(1);
await new Promise(resolve => setTimeout(resolve, 0));
asyncTestPassed();
throw new TypeError("expected after the run loop has run");
