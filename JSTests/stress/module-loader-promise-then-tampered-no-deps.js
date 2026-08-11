//@ runDefault

// Test that module loading works for a module with NO dependencies even when
// Promise.prototype.then is completely replaced. The initial module goes
// through loadModule's first overload which chains on the already-fulfilled
// fetch promise directly, so the tampered .then is never invoked for the
// critical module loading path.

var originalThen = Promise.prototype.then;
Promise.prototype.then = function (onFulfilled, onRejected) {
    onFulfilled("tampered");
};

originalThen.call(
    import("./resources/module-loader-promise-then-tampered-no-deps.js"),
    function (ns) {
        // The module loading itself succeeds because the fetch promise was
        // already fulfilled. But the import() result promise resolution
        // goes through the tampered .then, so we may get a tampered value.
        // The test passes as long as JSC doesn't crash.
    },
    function (e) {
        // Rejection is also acceptable — no crash is the requirement.
    }
);
