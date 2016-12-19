import Builder from '../Builder.js'

const pageSize = 64 * 1024;
const numPages = 10;

const builder = (new Builder())
    .Type().End()
    .Import()
        .Memory("a", "b", {initial: numPages})
    .End()
    .Function().End()
    .Export().Function("foo").End()
    .Code()
        .Function("foo", {params: ["i32"], ret: "i32"})
            .GetLocal(0)
            .I32Load(2, 0)
            .Return()
        .End()
    .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
const foo = new WebAssembly.Instance(module, {a: {b: new WebAssembly.Memory({initial: numPages})}}).exports.foo;

function assert(b) {
    if (!b)
        throw new Error("Bad")
}

function wasmFrameCountFromError(e) {
    let stackFrames = e.stack.split("\n").filter((s) => s.indexOf("<wasm>@[wasm code]") !== -1);
    return stackFrames.length;
}

for (let i = 0; i < 1000; i++) {
    let threw = false;
    try {
        foo(numPages * pageSize + 1);
    } catch(e) {
        assert(e instanceof WebAssembly.RuntimeError);
        assert(e.message === "Out of bounds memory access");
        threw = true;
        assert(wasmFrameCountFromError(e) === 2);
    }
    assert(threw);
}
