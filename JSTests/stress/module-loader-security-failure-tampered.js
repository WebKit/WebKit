//@ runDefault

// Test that module loading failure (non-existent module) works correctly when
// Promise.prototype.then is tampered. The rejection path goes through
// rejectWithCaughtException on internal promises. This verifies:
// 1. Error handling still works (rejection propagates)
// 2. No internal objects leak in rejection values
// 3. No crash

function fail(msg) { print("FAIL: " + msg); $vm.abort(); }

var originalThen = Promise.prototype.then;

function isInternalType(desc) {
    if (/ModuleRecord(?!.*NamespaceObject)/.test(desc))
        return true;
    var types = ["JSSourceCode", "ModuleRegistryEntry", "ModuleLoadingContext",
                 "ModuleGraphLoadingState", "ModuleLoaderPayload"];
    for (var i = 0; i < types.length; i++) {
        if (desc.indexOf(types[i]) !== -1)
            return true;
    }
    return false;
}

var leakedValues = [];

Promise.prototype.then = function (onFulfilled, onRejected) {
    var desc = describe(this);
    if (isInternalType(desc))
        leakedValues.push("then-this: " + desc);

    var wrappedFulfilled = onFulfilled ? function (val) {
        if (val !== null && val !== undefined) {
            var valDesc = describe(val);
            if (isInternalType(valDesc))
                leakedValues.push("fulfilled-arg: " + valDesc);
        }
        return onFulfilled(val);
    } : onFulfilled;

    var wrappedRejected = onRejected ? function (err) {
        if (err !== null && err !== undefined) {
            var errDesc = describe(err);
            if (isInternalType(errDesc))
                leakedValues.push("rejected-arg: " + errDesc);
        }
        return onRejected(err);
    } : onRejected;

    return originalThen.call(this, wrappedFulfilled, wrappedRejected);
};

originalThen.call(
    import("./resources/nonexistent-module-security-test-xyz.js"),
    function (ns) {
        fail("Import of non-existent module should not have succeeded");
    },
    function (e) {
        // Rejection is expected. Verify the error is a proper Error object.
        if (!(e instanceof Error))
            fail("Expected rejection with Error, got: " + typeof e);
        if (leakedValues.length > 0)
            fail("Internal objects leaked in error path: " + leakedValues.join(", "));
    }
);
