//@ runDefault

// Test that a deep import chain (A -> B -> C) works correctly when
// Promise.prototype.then is tampered. Each dependency goes through the full
// fetch -> parse -> ModuleRegistryFetchSettled -> ModuleRegistryModuleSettled
// pipeline, passing JSSourceCode* and AbstractModuleRecord* through internal
// promises at each step. This test verifies none of them leak.

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
    import("./resources/module-loader-security-deep-a.js"),
    function (ns) {
        if (ns.a !== "a")
            fail("Expected ns.a to be 'a', got " + ns.a);
        if (ns.b !== "b")
            fail("Expected ns.b to be 'b', got " + ns.b);
        if (ns.c !== "c")
            fail("Expected ns.c to be 'c', got " + ns.c);
        if (leakedValues.length > 0)
            fail("Internal objects leaked: " + leakedValues.join(", "));
    },
    function (e) {
        fail("Module loading should not have failed: " + e);
    }
);
