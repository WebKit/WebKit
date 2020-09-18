function bar(testArgs) {
    (() => {
        try {
            testArgs.func.call(1);
        } catch (e) {
            if (!testArgs.qux) {
                return e == testArgs.qux;
            }
        }
    })();
}
for (var i = 0; i < 100000; i++) {
    [
        {
            func: ()=>{},
        },
        {
            func: Int8Array.prototype.values,
            foo: 0
        },
        {
            func: Int8Array.prototype.values,
            qux: 2
        },
        {
            func: Int8Array.prototype.values,
        },
    ].forEach(bar);
}
