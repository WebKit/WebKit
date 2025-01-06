//@ requireOptions("--thresholdForJITSoon=10 --thresholdForJITAfterWarmUp=10 --thresholdForOptimizeAfterWarmUp=100 --useConcurrentJIT=0")
class C0 { }
const v1 = new C0();
for (let i = 0; i < 10; i++) {
        const v25 = new SharedArrayBuffer(3614, {"maxByteLength": 3614,});
        const v27 = new Int32Array(v25);
        let originalPrototype = Object.getPrototypeOf(v27);
        let handler = {
            get(target, key, receiver) {
                if ( receiver === v27) return originalPrototype;
            }
        };
        let newPrototype = new Proxy(originalPrototype, handler);
        Object.setPrototypeOf(v27, newPrototype);
        function f28() {
            try { v1.m(); } catch (e) {}
            for (let v31 = 0; v31 < 5; v31++) {}
        }
        v27[Symbol.toPrimitive] = f28;
        createGlobalObject().Atomics.waitAsync(v27, 200, v27, 200);
}
gc();
