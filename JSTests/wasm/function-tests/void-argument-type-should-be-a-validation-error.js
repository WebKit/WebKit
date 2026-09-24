import * as assert from '../assert.js';
import Builder from '../Builder.js';

function getBinary(params) {
    const builder = (new Builder())
    builder.Type().End()
        .Function().End()
        .Memory().InitialMaxPages(1, 1).End()
        .Export()
            .Function("callFunc")
        .End()
        .Code()
            .Function("callFunc", { params, ret: "void" })
                .Return()
            .End()
        .End();
    return builder.WebAssembly().get();
}

assert.compileError(getBinary(["i32", "void"]), "WebAssembly.Module doesn't parse at byte 15: can't get 1th argument Type");
assert.compileError(getBinary(["void"]), "WebAssembly.Module doesn't parse at byte 14: can't get 0th argument Type");
assert.compileError(getBinary(["i32", "void", "i32"]), "WebAssembly.Module doesn't parse at byte 15: can't get 1th argument Type");
