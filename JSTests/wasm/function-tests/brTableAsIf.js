import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["i32"], ret: "i32" }, ["i32"])
    .Block("void", (b) =>
           b.Block("void", (b) =>
                   b.GetLocal(0)
                   .BrTable(1, 0)
                   .I32Const(21)
                   .Return()
                  ).I32Const(20)
           .Return()
          ).I32Const(22)
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 1, [[{type: "i32", value: 22 }, [{ type: "i32", value: 0 }]],
                                       [{type: "i32", value: 20 }, [{ type: "i32", value: 1 }]],
                                       [{type: "i32", value: 20 }, [{ type: "i32", value: 11 }]],
                                       [{type: "i32", value: 20 }, [{ type: "i32", value: -100 }]]
                                      ]);
