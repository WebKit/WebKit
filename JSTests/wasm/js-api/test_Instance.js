import * as assert from '../assert.js';
import Builder from '../Builder.js';

(function EmptyModule() {
    const builder = new Builder();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);
    assert.instanceof(instance, WebAssembly.Instance);
})();

(function ExportedAnswerI32() {
    const builder = (new Builder())
        .Type().End()
        .Function().End()
        .Export()
            .Function("answer")
        .End()
        .Code()
            .Function("answer", { params: [], ret: "i32" })
                .I32Const(42)
                .Return()
            .End()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);
    const result = instance.exports.answer();
    assert.isA(result, "number");
    assert.eq(result, 42);
})();

// FIXME exporting a property with a name that's a number doesn't work https://bugs.webkit.org/show_bug.cgi?id=168857
/*
(function ExportedNumber() {
    for (let i = 0; i <= 129; ++i) {
        const name = i.toString();
        const builder = (new Builder())
             .Type().End()
             .Function().End()
             .Export()
                 .Function(name)
             .End()
             .Code()
                 .Function(name, { params: [], ret: "i32" })
                 .I32Const(i)
                 .Return()
             .End()
             .End();
        const bin = builder.WebAssembly().get();
        const module = new WebAssembly.Module(bin);
        const instance = new WebAssembly.Instance(module);
        const result = instance.exports[name]();
        assert.isA(result, "number");
        assert.eq(result, i);
    }
})();
*/

const wasmModuleWhichImportJS = () => {
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Function("imp", "func", { params: ["i32"] })
        .End()
        .Function().End()
        .Export()
            .Function("changeCounter")
        .End()
        .Code()
            .Function("changeCounter", { params: ["i32"] })
                .I32Const(42)
                .GetLocal(0)
                .I32Add()
                .Call(0) // Calls func(param[0] + 42).
            .End()
        .End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    return module;
};

(function MonomorphicImport() {
    let counter = 0;
    const counterSetter = v => counter = v;
    const module = wasmModuleWhichImportJS();
    const instance = new WebAssembly.Instance(module, { imp: { func: counterSetter } });
    for (let i = 0; i < 4096; ++i) {
        // Invoke this a bunch of times to make sure the IC in the wasm -> JS stub works correctly.
        instance.exports.changeCounter(i);
        assert.eq(counter, i + 42);
    }
})();

(function Polyphic2Import() {
    let counterA = 0;
    let counterB = undefined;
    const counterASetter = v => counterA = v;
    const counterBSetter = v => counterB = { valueB: v };
    const module = wasmModuleWhichImportJS();
    const instanceA = new WebAssembly.Instance(module, { imp: { func: counterASetter } });
    const instanceB = new WebAssembly.Instance(module, { imp: { func: counterBSetter } });
    for (let i = 0; i < 2048; ++i) {
        instanceA.exports.changeCounter(i);
        assert.isA(counterA, "number");
        assert.eq(counterA, i + 42);
        instanceB.exports.changeCounter(i);
        assert.isA(counterB, "object");
        assert.eq(counterB.valueB, i + 42);
    }
})();

(function Polyphic3Import() {
    let counterA = 0;
    let counterB = undefined;
    let counterC = undefined;
    const counterASetter = v => counterA = v;
    const counterBSetter = v => counterB = { valueB: v };
    const counterCSetter = v => counterC = { valueC: v };
    const module = wasmModuleWhichImportJS();
    const instanceA = new WebAssembly.Instance(module, { imp: { func: counterASetter } });
    const instanceB = new WebAssembly.Instance(module, { imp: { func: counterBSetter } });
    const instanceC = new WebAssembly.Instance(module, { imp: { func: counterCSetter } });
    for (let i = 0; i < 2048; ++i) {
        instanceA.exports.changeCounter(i);
        assert.isA(counterA, "number");
        assert.eq(counterA, i + 42);
        instanceB.exports.changeCounter(i);
        assert.isA(counterB, "object");
        assert.eq(counterB.valueB, i + 42);
        instanceC.exports.changeCounter(i);
        assert.isA(counterC, "object");
        assert.eq(counterC.valueC, i + 42);
    }
})();

(function VirtualImport() {
    const num = 10; // It's definitely going virtual at 10!
    let counters = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    const counterSetters = [
        v => counters[0] = v,
        v => counters[1] = v + 1,
        v => counters[2] = v + 2,
        v => counters[3] = v + 3,
        v => counters[4] = v + 4,
        v => counters[5] = v + 5,
        v => counters[6] = v + 6,
        v => counters[7] = v + 7,
        v => counters[8] = v + 8,
        v => counters[9] = v + 9,
    ];
    assert.eq(counters.length, num);
    assert.eq(counterSetters.length, num);
    const module = wasmModuleWhichImportJS();
    let instances = [];
    for (let i = 0; i < num; ++i)
        instances[i] = new WebAssembly.Instance(module, { imp: { func: counterSetters[i] } });
    for (let i = 0; i < 2048; ++i) {
        for (let j = 0; j < num; ++j) {
            instances[j].exports.changeCounter(i);
            assert.isA(counters[j], "number");
            assert.eq(counters[j], i + 42 + j);
        }
    }
})();
