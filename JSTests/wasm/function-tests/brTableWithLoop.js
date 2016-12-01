import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["i32"], ret: "i32" }, ["i32"])
    .Loop("void")
    .Block("void", (b) =>

           b.GetLocal(0)
           .GetLocal(1)
           .I32Const(1)
           .I32Add()
           .SetLocal(1)
           .GetLocal(0)
           .I32Const(1)
           .I32Sub()
           .SetLocal(0)
           .BrTable(0, 1))

    .End()
    .GetLocal(1)
    .End()
const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 1, [[{type: "i32", value: 1 }, [{ type: "i32", value: 0 }]],
                                       [{type: "i32", value: 2 }, [{ type: "i32", value: 1 }]]
                                      ]);
