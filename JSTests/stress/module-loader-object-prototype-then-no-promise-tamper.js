//@ runDefault

// Test that module loading works correctly when Object.prototype.then is set
// to a callable function. Module records have null prototype and JSSourceCode
// is a JSCell (not JSObject), so Object.prototype.then should not interfere
// with module loading.
//
// Note: This test does NOT tamper with Promise.prototype.then, so the
// promiseThenWatchpointSet remains valid. Even though Object.prototype.then
// is callable, isDefinitelyNonThenable() returns true (watchpoint valid)
// for non-Promise objects.

Object.prototype.then = function (resolve, reject) {
    resolve("tampered");
};

import("./resources/module-loader-promise-then-tampered-target.js").then(
    function (ns) {
        if (ns.value !== 42) {
            print("Expected ns.value to be 42, got " + ns.value);
            $vm.abort();
        }
    },
    function (e) {
        print("Module loading should not have failed: " + e);
        $vm.abort();
    }
);
