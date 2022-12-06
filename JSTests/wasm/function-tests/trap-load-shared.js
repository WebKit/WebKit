//@ skip if $architecture != "arm64" && $architecture != "x86_64"

import Builder from '../Builder.js'
import * as assert from '../assert.js'

const pageSize = 64 * 1024;
const numPages = 10;

const builder = (new Builder())
    .Type().End()
    .Import()
        .Memory("a", "b", {initial: numPages, maximum: numPages * 2, shared: true})
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
const foo = new WebAssembly.Instance(module, {a: {b: new WebAssembly.Memory({initial: numPages, maximum: numPages * 2, shared: true})}}).exports.foo;

function wasmFrameCountFromError(e) {
    let stackFrames = e.stack.split("\n").filter((s) => s.indexOf("wasm-") !== -1);
    return stackFrames.length;
}

for (let i = 0; i < 1000; i++) {
    const e = assert.throws(() => foo(numPages * pageSize + 1), WebAssembly.RuntimeError, "Out of bounds memory access");
    assert.eq(wasmFrameCountFromError(e), 2);
}
