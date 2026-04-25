//@ runDefault

// Test that import() of a module exporting "then" correctly uses thenable
// unwrapping per spec (ContinueDynamicImport step 6.d.ii). The namespace
// is a thenable and its "then" export should receive proper resolve/reject
// functions, not internal objects.
//
// The validation of resolve/reject and this-value is done inside the
// module's exported "then" function itself (see the helper module).

function fail(msg) { print("FAIL: " + msg); $vm.abort(); }

var originalThen = Promise.prototype.then;

originalThen.call(
    import("./resources/module-loader-security-thenable-namespace-module.js"),
    function (result) {
        // The "then" export calls resolve({value, isNamespaceResult}) to break
        // the thenable cycle. With the isDefinitelyNonThenable fix, thenable
        // unwrapping happens even when the watchpoint is valid.
        if (result.value !== 42)
            fail("Expected result.value to be 42, got " + result.value);
        if (result.isNamespaceResult !== true)
            fail("Expected isNamespaceResult to be true, got " + result.isNamespaceResult);
    },
    function (e) {
        fail("Module loading should not have failed: " + e);
    }
);
