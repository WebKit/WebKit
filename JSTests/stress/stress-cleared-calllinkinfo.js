//@ slow!
function runNearStackLimit(f) {
    function t() {
        try {
            return t();
        } catch (e) {
            String(e);
            return f();
        }
    }
    return t()
}
function repeat(func, count) {
    for (let i = 0; i < count; i++) {
        try {
            func(i);
        } catch (e) {}
    }
}
let array = [Error, String, RegExp, {}, class cls {}];
for (let item of array) {
    runNearStackLimit(() => {
        return repeat(void 0, 30);
    })
}

repeat(function (v) {
    let func = new Proxy(function () {
        gc();
    }, {});
    runNearStackLimit(() => {
        new func();
    })
}, 5);
