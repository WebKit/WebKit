import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    const returns = [];
    const maxFunctionReturns = 50000;
    for (let i = 0; i < maxFunctionReturns; ++i)
        returns[i] = "i32";
    let cont = b
        .Type().End()
        .Function().End()
        .Export()
            .Function("loop")
        .End()
        .Code()
            .Function("loop", { params: ["i32"], ret: returns });
    for (var i = 0; i < maxFunctionReturns; ++i)
        cont = cont.I32Const(1);
    cont = cont.Return().End().End();
    const bin = b.WebAssembly().get();
    var exception;
    try {
        const module = new WebAssembly.Module(bin);
    } catch (e) {
        exception = "" + e;
    }

    assert.eq(exception, "CompileError: WebAssembly.Module doesn't parse at byte 19: return count of Type at index 0 is too big 50000 maximum 1000 (evaluating 'new WebAssembly.Module(bin)')");
}
