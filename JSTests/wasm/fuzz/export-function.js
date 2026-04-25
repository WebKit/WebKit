import * as assert from '../assert.js';
import Builder from '../Builder.js';

const numRandomIterations = 128;

// Generate wasm export functions of arity [0, max), using each valid
// WebAssembly type as parameters. Make sure this number is high enough to force
// non-register calls.
const maxArities = 64;

// Calls a "check" function for each parameter received.
const paramExporter = (params, returnedParam, imports) => {
    const ret = params.length ? params[returnedParam] : "void";
    let builder = (new Builder())
        .Type().End()
        .Import()
            .Function("imp", "checki32", { params: ["i32"] })
            .Function("imp", "checkf32", { params: ["f32"] })
            .Function("imp", "checkf64", { params: ["f64"] })
        .End()
        .Function().End()
        .Export()
            .Function("func")
        .End()
        .Code()
            .Function("func", { params: params, ret: ret });
    for (let i = 0; i < params.length; ++i) {
        builder = builder.GetLocal(i);
        switch (params[i]) {
        case "i32": builder = builder.Call(0); break;
        case "f32": builder = builder.Call(1); break;
        case "f64": builder = builder.Call(2); break;
        default: throw new Error(`Unexpected type`);
        }
    }
    if (ret !== "void")
        builder = builder.GetLocal(returnedParam);
    builder = builder.Return().End().End();
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    return new WebAssembly.Instance(module, { imp: imports });
};

var buffer = new ArrayBuffer(8);
var viewi16 = new Int16Array(buffer);
var viewi32 = new Int32Array(buffer);
var viewf32 = new Float32Array(buffer);
var viewf64 = new Float64Array(buffer);
const random16 = () => (Math.random() * (1 + 0xffff)) | 0;
const setBuffer = () => {
    for (let i = 0; i < 4; ++i)
        viewi16[i] = random16();
};
const types = [
    { type: "i32", generate: () => { setBuffer(); return viewi32[0]; } },
    // i64 isn't supported.
    { type: "f32", generate: () => { setBuffer(); return viewf32[0]; } },
    { type: "f64", generate: () => { setBuffer(); return viewf64[0]; } },
];

for (let iteration = 0; iteration < numRandomIterations; ++iteration) {
    const arity = (Math.random() * (maxArities + 1)) | 0;
    let params = [];
    let args = [];
    for (let a = 0; a < arity; ++a) {
        const type =( Math.random() * types.length) | 0;
        params.push(types[type].type);
        args.push(types[type].generate());
    }
    let numChecked = 0;
    const imports = {
        checki32: v => assert.eq(v, args[numChecked++]),
        checkf32: v => assert.eq(v, args[numChecked++]),
        checkf64: v => assert.eq(v, args[numChecked++]),
    };
    const returnedParam = (Math.random() * params.length) | 0;
    const instance = paramExporter(params, returnedParam, imports);
    const result = instance.exports.func(...args);
    assert.eq(result, args.length ? args[returnedParam] : undefined);
}
