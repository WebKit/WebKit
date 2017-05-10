import Builder from '../Builder.js'

function assert(b) {
    if (!b)
        throw new Error("Bad");
}

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
assert(!stacktrace);
for (let i = 0; i < 10000; ++i) {
    instance.exports.entry();
    assert(stacktrace);
    stacktrace = stacktrace.split("\n");
    assert(stacktrace[0].indexOf("imp") !== -1); // the arrow function import named "imp".
    assert(stacktrace[1] === "wasm function@[wasm code]"); // the wasm->js stub
    assert(stacktrace[2] === "wasm function: 4@[wasm code]");
    assert(stacktrace[3] === "wasm function: 2@[wasm code]");
    assert(stacktrace[4] === "wasm function: 3@[wasm code]");
    assert(stacktrace[5] === "wasm function: 1@[wasm code]");
    assert(stacktrace[6] === "wasm function@[wasm code]"); // wasm entry

    stacktrace = null;
}
