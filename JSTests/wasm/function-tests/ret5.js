import Builder from '../Builder.js'

const b = new Builder();
let code = b.Type().End()
    .Function().End()
    .Code();

code.Function({ params: [], ret: "i32" })
    .I32Const(5)
    .Return()
    .End()
    .End();
const bin = b.WebAssembly();
bin.trim();
testWasmModuleFunctions(bin.get(), 1, [[{type: "i32", value: 5 }, []]]);
