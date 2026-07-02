//@ runDefault

// Test that import() of a module exporting "then" works correctly even when
// Promise.prototype.then is ALSO tampered. This exercises both:
// 1. The internal pipeline (which should bypass the tampered then)
// 2. The final user-visible resolve() in importModuleNamespace (spec-required)

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
    import("./resources/module-loader-security-thenable-namespace-module.js"),
    function (result) {
        if (result.value !== 42)
            fail("Expected result.value to be 42, got " + result.value);
        if (leakedValues.length > 0)
            fail("Internal objects leaked: " + leakedValues.join(", "));
    },
    function (e) {
        fail("Module loading should not have failed: " + e);
    }
);
