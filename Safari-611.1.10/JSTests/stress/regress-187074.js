// This test should not crash.
var done = false;

function runNearStackLimit(f) {
    function t() {
        try {
            return t();
        } catch (e) {
            if (!done)
                return f();
        }
    }
    return t()
}

runNearStackLimit(() => {
    done = true;
    eval("({ __proto__ : [], __proto__: {} })")
});

