import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["i32"], ret: "i32" }, [])
    .Block("i32", (b) =>
           b.GetLocal(0)
           .I32Const(0)
           .I32Eq()
           .If("i32")
               .I32Const(1)
           .Else()
               .I32Const(2)
           .Br(1)
           .End()
          ).End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 1, [[{type: "i32", value: 1 }, [{ type: "i32", value: 0 }]],
                                       [{type: "i32", value: 2 }, [{ type: "i32", value: 1 }]]
                                      ]);
