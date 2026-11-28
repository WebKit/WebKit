// A microtask queued before terminate() must not run after it. Without
// VM::forbidExecutionOnTermination(), evaluate() consumes the termination exception on the
// way out of the worker's top-level script, leaving execution permitted, and runJSC's
// drainMicrotasks() then runs the queued microtask.
const worker = new Worker(`
    const flag = new Int32Array(new SharedArrayBuffer(4));
    Promise.resolve().then(() => {
        Atomics.store(flag, 0, 1);
        Atomics.notify(flag, 0);
    });
    postMessage(flag.buffer);
    while (true) { }
`, { source: "string" });

worker.onmessage = function (event) {
    const flag = new Int32Array(event.data);
    worker.terminate();
    if (Atomics.wait(flag, 0, 0, 20) !== "timed-out")
        throw new Error("a microtask queued before terminate() ran after it");
};
