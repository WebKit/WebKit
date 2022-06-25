function foo() {
    let arr1 = new Int32Array(new SharedArrayBuffer(12));
    Object.defineProperty(Array.prototype, "0", {
        get: () => {
            function* foo2() {
                throw "stuff";
            };
            foo2();
        },
        set: () => { }
    });
    arr1.sort();
}

try {
    foo()
} catch (y) {
}
