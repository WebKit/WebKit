function useAllMemory() {
    try {
        const a = [];
        a.__proto__ = {};
        Object.defineProperty(a, 0, { get: foo });
        Object.defineProperty(a, 80000000, {});
        function foo() {
            new Uint8Array(a);
        }
        new Promise(foo).catch(() => {});
        while(1) {
            new ArrayBuffer(1000);
        }
    } catch { }
}

var exception = null;
useAllMemory();
try {
    for (let i = 0; i < 1000; i++) {
        CSS.paintWorklet.addModule('');
    }
} catch (e) {
    exception = e;
}

// Exception may not be thrown, depends on GC timing.
if (exception !== null) {
    if (exception != "RangeError: Out of memory")
        throw "FAIL: expect: 'RangeError: Out of memory', actual: '" + exception + "'";
}
