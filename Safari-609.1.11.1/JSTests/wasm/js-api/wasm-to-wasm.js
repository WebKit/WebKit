import * as assert from '../assert.js';
import Builder from '../Builder.js';

const callerTopBits = 0xC0FEBEEF;
const innerReturnHi = 0xDEADFACE;
const innerReturnLo = 0xC0FEC0FE;

const callerModule = () => {
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Function("exports", "callMe", { params: ["i64"], ret: "i64" })
        .End()
        .Function().End()
        .Export()
            .Function("entry")
        .End()
        .Code()
            .Function("entry", { params: ["i32"], ret: "i32" }, ["i64"])
                .I32Const(callerTopBits).I64ExtendUI32().I32Const(32).I64ExtendUI32().I64Shl() // ((i64)callerTopBits) << 32
                .GetLocal(0).I64ExtendUI32()
                .I64Or() // value: param | (((i64)callerTopBits << 32))
                .Call(0) // Calls exports.callMe(param | (((i64)callerTopBits) << 32)).
                .TeeLocal(1).I32WrapI64() // lo: (i32)callResult
                .GetLocal(1).I32Const(32).I64ExtendUI32().I64ShrU().I32WrapI64() // hi: (i32)(callResult >> 32)
                .I32Xor()
                .Return()
            .End()
        .End();
    return new WebAssembly.Module(builder.WebAssembly().get());
};

const calleeModule = () => {
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Function("imp", "func", { params: ["i32", "i32"], ret: "i32" })
        .End()
        .Function().End()
        .Export()
            .Function("callMe")
        .End()
        .Code()
            .Function("callMe", { params: ["i64"], ret: "i64" })
                .GetLocal(0).I32WrapI64() // lo: (i32)param
                .GetLocal(0).I32Const(32).I64ExtendUI32().I64ShrU().I32WrapI64() // hi: (i32)(param >> 32)
                .Call(0) // Calls imp.func with the 64-bit value as i32 { hi, lo }.
                .Drop()
                .I32Const(innerReturnHi).I64ExtendUI32().I32Const(32).I64ExtendUI32().I64Shl().I32Const(innerReturnLo).I64ExtendUI32().I64Or() // ((i64)hi << 32) | (i64)lo
                .Return()
            .End()
        .End();
    return new WebAssembly.Module(builder.WebAssembly().get());
};

(function WasmToWasm() {
    let value;
    const func = (hi, lo) => { value = { hi: hi, lo: lo }; return hi ^ lo; };
    const callee = new WebAssembly.Instance(calleeModule(), { imp: { func: func } });
    const caller = new WebAssembly.Instance(callerModule(), callee);
    for (let i = 0; i < 4096; ++i) {
        assert.eq(caller.exports.entry(i), innerReturnHi ^ innerReturnLo);
        assert.eq(value.lo >>> 0, callerTopBits);
        assert.eq(value.hi >>> 0, i);
    }
})();

// FIXME test the following https://bugs.webkit.org/show_bug.cgi?id=166625
// - wasm->wasm using 32-bit things (including float), as well as 64-bit NaNs that don't get canonicalized
// - Do a throw two-deep
// - Check that the first wasm's instance is back in OK state (with table or global?)
// - Test calling through a Table
