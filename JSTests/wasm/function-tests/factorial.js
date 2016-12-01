import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["i32"], ret: "i32" }, [])
    .GetLocal(0)
    .I32Const(0)
    .I32Eq()
    .If("void", b =>
        b.I32Const(1)
        .Return()
       )
        .GetLocal(0)
    .GetLocal(0)
    .I32Const(1)
    .I32Sub()
    .Call(0)
    .I32Mul()
    .Return()
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 1, [[{type: "i32", value: 1 }, [{ type: "i32", value: 0 }]],
                                       [{type: "i32", value: 1 }, [{ type: "i32", value: 1 }]],
                                       [{type: "i32", value: 2 }, [{ type: "i32", value: 2 }]],
                                       [{type: "i32", value: 24 }, [{ type: "i32", value: 4 }]],
                                      ]);
