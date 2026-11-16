function runNearStackLimit(f) {
    function t() {
        try {
            return t();
        } catch (e) {
            return f();
        }
    }
    return t()
}

function test() {
    function f(arg) {
        let loc = arg;
        try {
            loc.p = 0;
        } catch (e) {}
        arg.p = 1;
    }
    let obj = {};
    runNearStackLimit(() => {
        return f(obj);
    });
}
for (let i = 0; i < 50; ++i)
    test();
