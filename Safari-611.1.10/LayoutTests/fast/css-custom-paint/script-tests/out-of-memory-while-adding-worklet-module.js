if ($vm.isGigacageEnabled()) {
    window.allocated = [];
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
                window.allocated.push(new ArrayBuffer(1000));
            }
        } catch { }
    }

    async function runTest() {
        let promises = []
        try {
            for (let i = 0; i < 5000; i++)
                await CSS.paintWorklet.addModule('');
        } catch (e) {
            if (e != "RangeError: Out of memory")
                document.write("FAIL: expect: 'RangeError: Out of memory', actual: '" + e + "'");
            else
                document.write("PASS: threw a RangeError: Out of memory exception");
        }
    }

    var exception;
    useAllMemory();
    runTest();
}
