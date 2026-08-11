// Test that module loading works correctly for a module with no dependencies
// when Promise.prototype.then is tampered. The initial module load goes through
// loadModule (first overload) which chains directly on the already-fulfilled
// fetch promise via performPromiseThenWithInternalMicrotask. Since the fetch
// promise is already fulfilled, the value is taken directly from the promise's
// internal state, bypassing resolvePromise() entirely.

export var value = 42;
