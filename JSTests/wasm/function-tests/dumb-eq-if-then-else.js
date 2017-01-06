import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Memory().InitialMaxPages(1, 1).End()
    .Code()
    .Function({ params: ["i32", "i32"], ret: "i32" }, [])
    .GetLocal(0)
    .GetLocal(1)
    .I32Eq()
    .If("void", b =>
        b.Br(0)
        .Else()
            .I32Const(1)
        .Return()
       )
        .I32Const(0)
    .Return()
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 1, [[{type: "i32", value: 1 }, [{ type: "i32", value: 0 }, { type: "i32", value: 1 }]],
                                       [{type: "i32", value: 1 }, [{ type: "i32", value: 1 }, { type: "i32", value: 0 }]],
                                       [{type: "i32", value: 1 }, [{ type: "i32", value: 2 }, { type: "i32", value: 1 }]],
                                       [{type: "i32", value: 1 }, [{ type: "i32", value: 1 }, { type: "i32", value: 2 }]],
                                       [{type: "i32", value: 0 }, [{ type: "i32", value: 2 }, { type: "i32", value: 2 }]],
                                       [{type: "i32", value: 0 }, [{ type: "i32", value: 1 }, { type: "i32", value: 1 }]],
                                       [{type: "i32", value: 0 }, [{ type: "i32", value: 0 }, { type: "i32", value: 0 }]],
                                       [{type: "i32", value: 1 }, [{ type: "i32", value: 2 }, { type: "i32", value: 6 }]],
                                       [{type: "i32", value: 1 }, [{ type: "i32", value: 100 }, { type: "i32", value: 6 }]]
                                      ]);
