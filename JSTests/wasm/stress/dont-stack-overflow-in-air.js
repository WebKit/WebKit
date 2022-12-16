//@ requireOptions("--maxPerThreadStackUsage=512000", "--useWebAssemblySIMD=false")

import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const locals = [];
    const numLocals = 1000;
    for (let i = 0; i < numLocals; ++i)
        locals[i] = "f64";

    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Export()
            .Function("test")
        .End()
        .Code()
        .Function("test", { params: ["i32"], ret: "void" }, locals)
            .GetLocal(0)
            .I32Const(0)
            .I32Eq()
            .BrIf(0)
            .GetLocal(0)
            .I32Const(1)
            .I32Sub()
            .Call(0)
        .End()
        .End();

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    // This should not stack overflow
    instance.exports.test(25);
}
