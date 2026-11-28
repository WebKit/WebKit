//@ requireOptions("--exception=Error")

// An uncaught exception means the shell is exiting, so it must not wait on keep-alives
// such as a live Worker. Without that, runJSC enters the run loop after reporting the
// exception and the worker's DeferredWorkTimer ticket pins it forever. An idle worker is
// enough to pin it; the worker does not need to be running anything.
new Worker("", { source: "string" });

throw new Error("should exit with this exception rather than waiting on the worker");
