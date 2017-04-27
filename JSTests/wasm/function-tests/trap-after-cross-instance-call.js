import Builder from '../Builder.js'
import * as assert from '../assert.js'

const pageSize = 64 * 1024;
const numPages = 10;

const builder = (new Builder())
    .Type().End()
    .Import()
        .Memory("a", "b", {initial: numPages})
        .Function("foo", "bar", { params: [], ret: "void" })
    .End()
    .Function().End()
    .Export().Function("foo").End()
    .Code()
        .Function("foo", {params: ["i32"], ret: "i32"})
            .Call(0)
            .GetLocal(0)
            .I32Load(2, 0)
            .Return()
        .End()
    .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);

let importObject = {a: {b: new WebAssembly.Memory({initial: numPages})}};

{
    const builder = (new Builder())
          .Type().End()
          .Import()
              .Memory("a", "b", {initial: numPages})
          .End()
          .Function().End()
          .Export().Function("bar").End()
          .Code()
              .Function("bar", { params: [], ret: "void" })
                  .Return()
              .End()
          .End();

    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);

    importObject.foo = new WebAssembly.Instance(module, {a: {b: new WebAssembly.Memory({initial: numPages})}}).exports
}

let foo1 = new WebAssembly.Instance(module, importObject).exports.foo;
importObject.foo = { bar() { } };
let foo2 = new WebAssembly.Instance(module, importObject).exports.foo;


function wasmFrameCountFromError(e) {
    let stackFrames = e.stack.split("\n").filter((s) => s.indexOf("wasm function") !== -1);
    return stackFrames.length;
}

for (let i = 0; i < 1000; i++) {
    const e1 = assert.throws(() => foo1(numPages * pageSize + 1), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.eq(wasmFrameCountFromError(e1), 2);
    const e2 = assert.throws(() => foo2(numPages * pageSize + 1), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.eq(wasmFrameCountFromError(e2), 2);
}
