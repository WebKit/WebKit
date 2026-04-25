import * as assert from '../assert.js';
import Builder from '../Builder.js';

// Generate wasm export functions of arity [0, max), and call each of these
// export functions from JS with [0, max) parameters, for each valid WebAssembly
// type. Make sure this number is high enough to force non-register calls.
const maxArities = 64;

const paramExporter = (numParams, paramType, imports) => {
    let builder = (new Builder())
        .Type().End()
        .Import()
            .Function("imp", "check", { params: [paramType] })
        .End()
        .Function().End()
        .Export()
            .Function("func")
        .End()
        .Code()
          .Function("func", { params: Array(numParams).fill(paramType) });
    for (let i = 0; i < numParams; ++i)
        builder = builder.GetLocal(i).Call(0); // Call the import for each received parameter.
    builder = builder.Return().End().End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    return new WebAssembly.Instance(module, { imp: imports });
};

const types = [
    { type: "i32", value: 42, defaultWhenArityMismatch: 0 },
    // i64 isn't supported.
    { type: "f32", value: 32.0, defaultWhenArityMismatch: NaN },
    { type: "f64", value: 64.0, defaultWhenArityMismatch: NaN },
];

for (let type of types) {
    for (let wasmArity = 0; wasmArity < maxArities; ++wasmArity) {
        let numParamsCallingWith = undefined;
        let numChecked = 0;
        const check = value => {
            assert.isNumber(value);
            if (numParamsCallingWith <= wasmArity) {
                if (numChecked < numParamsCallingWith)
                    assert.eq(value, type.value);
                else
                    assert.eq(value, type.defaultWhenArityMismatch);
            }  else {
                if (numChecked < wasmArity)
                    assert.eq(value, type.value);
                else
                    assert.eq(value, type.defaultWhenArityMismatch);
            }
            ++numChecked;
        };
        const instance = paramExporter(wasmArity, type.type, { check: check });
        for (let callerArity = 0; callerArity < maxArities; ++callerArity) {
            numParamsCallingWith = callerArity;
            const params = Array(callerArity).fill(type.value);
            const result = instance.exports.func(...params);
            assert.isUndef(result);
            assert.eq(numChecked, wasmArity); // check() should be called as many times as the wasm function's arity.
            numChecked = 0; // Reset the check counter for each arity iteration.
        }
    }
}
