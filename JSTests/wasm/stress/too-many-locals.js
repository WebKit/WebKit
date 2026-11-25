//@ memoryHog!
import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    const locals = [];
    const maxFunctionLocals = 50000;
    const numLocals = maxFunctionLocals;
    for (let i = 0; i < numLocals; ++i)
        locals[i] = "i32";
    let cont = b
        .Type().End()
        .Function().End()
        .Export()
            .Function("loop")
        .End()
        .Code()
            .Function("loop", { params: ["i32"], ret: "i32" }, locals)
                .I32Const(1)
                .Return()
            .End()
        .End()

    const bin = b.WebAssembly().get();
    assert.compileError(bin,
        "WebAssembly.Module doesn't parse at byte 100002: Function's number of locals is too big 50001 maximum 50000, in function at index 0");
}
