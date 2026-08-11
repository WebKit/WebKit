//@ runDefault

// Test that module loading works correctly when Promise.prototype.then is
// replaced with the same value (invalidating the watchpoint but not changing
// behavior). The module should load successfully.

var originalThen = Promise.prototype.then;
Promise.prototype.then = originalThen;

originalThen.call(
    import("./resources/module-loader-promise-then-tampered-target.js"),
    function (ns) {
        if (ns.value !== 42) {
            print("Expected ns.value to be 42, got " + ns.value);
            $vm.abort();
        }
        if (ns.dep !== "from dependency") {
            print("Expected ns.dep to be 'from dependency', got " + ns.dep);
            $vm.abort();
        }
    },
    function (e) {
        print("Module loading should not have failed: " + e);
        $vm.abort();
    }
);
