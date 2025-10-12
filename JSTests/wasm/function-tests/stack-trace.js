import Builder from '../Builder.js'
import * as assert from '../assert.js'

const builder = (new Builder())
    .Type().End()
    .Import()
        .Function("imp", "f", { params: [], ret: "void" })
    .End()
    .Function().End()
    .Export().Function("entry").End()
    .Code()
         // idx 1
        .Function("entry", {params: []})
            .Call(3)
        .End()
        // idx 2
        .Function({params: []})
            .Call(4)
        .End()
        // idx 3
        .Function({params: []})
            .Call(2)
        .End()
        // idx 4
        .Function({params: []})
            .Call(0)
        .End()
    .End();

let stacktrace;
let imp = () => {
    stacktrace = (new Error).stack;
}


const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
let instance = new WebAssembly.Instance(module, {imp: {f: imp}});
assert.falsy(stacktrace);
for (let i = 0; i < wasmTestLoopCount; ++i) {
    instance.exports.entry();
    assert.truthy(stacktrace);
    stacktrace = stacktrace.split("\n");
    assert.truthy(stacktrace[0].indexOf("imp") !== -1); // the arrow function import named "imp".
    let found = false;
    for (let i = 0; i < stacktrace.length; ++i) {
        let str = stacktrace[i];
        if (str !== "4@wasm-function[4]")
            continue;
        found = true;
        assert.eq(stacktrace[i + 1], "2@wasm-function[2]");
        assert.eq(stacktrace[i + 2], "3@wasm-function[3]");
        assert.eq(stacktrace[i + 3], "1@wasm-function[1]");
    }
    assert.truthy(found);
    stacktrace = null;
}
