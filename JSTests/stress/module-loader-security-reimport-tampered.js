//@ runDefault

// Test that re-importing an already-loaded module works correctly when
// Promise.prototype.then is tampered AFTER the first import. The second
// import hits the module registry cache path (ModuleLoadingContext::Step::Cached)
// which uses jsSecureCast and fulfill(). This verifies the cached path
// doesn't leak internal objects.

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

// First import: clean, no tampering
originalThen.call(
    import("./resources/module-loader-promise-then-tampered-target.js"),
    function (ns) {
        if (ns.value !== 42)
            fail("First import: Expected ns.value to be 42, got " + ns.value);

        // Now tamper Promise.prototype.then
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

        // Second import: same module, but with tampered then
        originalThen.call(
            import("./resources/module-loader-promise-then-tampered-target.js"),
            function (ns2) {
                if (ns2.value !== 42)
                    fail("Second import: Expected ns2.value to be 42, got " + ns2.value);
                if (ns2.dep !== "from dependency")
                    fail("Second import: Expected ns2.dep, got " + ns2.dep);
                if (leakedValues.length > 0)
                    fail("Internal objects leaked on re-import: " + leakedValues.join(", "));
            },
            function (e) {
                fail("Second import should not have failed: " + e);
            }
        );
    },
    function (e) {
        fail("First import should not have failed: " + e);
    }
);
