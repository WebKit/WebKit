//@ runDefault

// Test using Object.defineProperty with a getter on Promise.prototype.then
// to detect ALL .then property lookups. This is more sophisticated than simple
// assignment: it detects property lookups (not just calls). After the fix,
// internal promises should NEVER have their .then looked up.

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

Object.defineProperty(Promise.prototype, "then", {
    get: function () {
        var selfDesc = describe(this);
        if (isInternalType(selfDesc))
            leakedValues.push("then-getter-this: " + selfDesc);
        return originalThen;
    },
    configurable: true
});

originalThen.call(
    import("./resources/module-loader-promise-then-tampered-target.js"),
    function (ns) {
        if (ns.value !== 42)
            fail("Expected ns.value to be 42, got " + ns.value);
        if (ns.dep !== "from dependency")
            fail("Expected ns.dep to be 'from dependency', got " + ns.dep);
        if (leakedValues.length > 0)
            fail("Internal object detected in .then getter: " + leakedValues.join(", "));
    },
    function (e) {
        fail("Module loading should not have failed: " + e);
    }
);
