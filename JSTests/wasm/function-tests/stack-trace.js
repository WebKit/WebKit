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
for (let i = 0; i < 10000; ++i) {
    instance.exports.entry();
    assert.truthy(stacktrace);
    stacktrace = stacktrace.split("\n");
    assert.truthy(stacktrace[0].indexOf("imp") !== -1); // the arrow function import named "imp".
    assert.eq(stacktrace[1], "wasm-stub@[wasm code]"); // the wasm->js stub
    assert.eq(stacktrace[2], "2C77E97D775063A868EF4437DA260245AFA94583.wasm-function[4]@[wasm code]");
    assert.eq(stacktrace[3], "2C77E97D775063A868EF4437DA260245AFA94583.wasm-function[2]@[wasm code]");
    assert.eq(stacktrace[4], "2C77E97D775063A868EF4437DA260245AFA94583.wasm-function[3]@[wasm code]");
    assert.eq(stacktrace[5], "2C77E97D775063A868EF4437DA260245AFA94583.wasm-function[1]@[wasm code]");
    assert.eq(stacktrace[6], "wasm-stub@[wasm code]"); // wasm entry

    stacktrace = null;
}
