import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Memory().InitialMaxPages(1, 1).End()
    .Code()
    .Function({ params: ["i32"], ret: "i32" }, [])
    .GetLocal(0)
    .I32Eqz()
    .End()


const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 1,
                        [[{type: "i32", value: 0 }, [{ type: "i32", value: 1 }]],
                         [{type: "i32", value: 1 }, [{ type: "i32", value: 0 }]]
                        ]
                       );
