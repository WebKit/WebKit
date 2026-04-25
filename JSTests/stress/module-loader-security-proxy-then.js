//@ runDefault

// Test using a Proxy-wrapped function as Promise.prototype.then to create
// the most thorough interception possible. The Proxy's apply trap logs all
// invocations including this value and all arguments, then validates via
// describe() that no internal objects appear.

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

Promise.prototype.then = new Proxy(originalThen, {
    apply: function (target, thisArg, args) {
        var thisDesc = describe(thisArg);
        if (isInternalType(thisDesc))
            leakedValues.push("proxy-this: " + thisDesc);

        for (var i = 0; i < args.length; i++) {
            if (args[i] !== null && args[i] !== undefined && typeof args[i] !== "function") {
                var argDesc = describe(args[i]);
                if (isInternalType(argDesc))
                    leakedValues.push("proxy-arg[" + i + "]: " + argDesc);
            }
        }

        // Wrap onFulfilled to also check resolution values
        var wrappedArgs = [];
        for (var i = 0; i < args.length; i++) {
            if (typeof args[i] === "function") {
                wrappedArgs.push((function (fn) {
                    return function (val) {
                        if (val !== null && val !== undefined) {
                            var valDesc = describe(val);
                            if (isInternalType(valDesc))
                                leakedValues.push("proxy-callback-arg: " + valDesc);
                        }
                        return fn(val);
                    };
                })(args[i]));
            } else {
                wrappedArgs.push(args[i]);
            }
        }

        return Reflect.apply(target, thisArg, wrappedArgs);
    }
});

originalThen.call(
    import("./resources/module-loader-promise-then-tampered-target.js"),
    function (ns) {
        if (ns.value !== 42)
            fail("Expected ns.value to be 42, got " + ns.value);
        if (ns.dep !== "from dependency")
            fail("Expected ns.dep to be 'from dependency', got " + ns.dep);
        if (leakedValues.length > 0)
            fail("Internal objects detected via Proxy: " + leakedValues.join(", "));
    },
    function (e) {
        fail("Module loading should not have failed: " + e);
    }
);
