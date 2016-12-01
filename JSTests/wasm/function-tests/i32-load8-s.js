import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Memory().InitialMaxPages(1, 1).End()
    .Code()
    .Function({ params: ["i32", "i32"], ret: "i32" }, [])
    .GetLocal(1)
    .GetLocal(0)
    .I32Store(2, 0)
    .GetLocal(1)
    .I32Load8S(2, 0)
    .Return()
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 1, [[{type: "i32", value: 0 }, [{ type: "i32", value: 0 }, { type: "i32", value: 10 }]],
                                       [{type: "i32", value: 100 }, [{ type: "i32", value: 100 }, { type: "i32", value: 112 }]],
                                       [{type: "i32", value: 0x40 }, [{ type: "i32", value: 1000000 }, { type: "i32", value: 10 }]]
                                      ]);
