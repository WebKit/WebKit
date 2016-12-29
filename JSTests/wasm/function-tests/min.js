import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["f32", "f32"], ret: "f32" }, [])
    .GetLocal(0)
    .GetLocal(1)
    .F32Min()
    .End()

    .Function({ params: ["f64", "f64"], ret: "f64" }, [])
    .GetLocal(0)
    .GetLocal(1)
    .F64Min()
    .End()



const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 2,
                        [[{type: "f32", value: 1.5 }, [{ type: "f32", value: 100 }, { type: "f32", value: 1.5 }]],
                         [{type: "f32", value: -1.5 }, [{ type: "f32", value: 100 }, { type: "f32", value: -1.5 }]],
                         [{type: "f32", value: -100 }, [{ type: "f32", value: -100 }, { type: "f32", value: 1.5 }]]
                        ],
                        [[{type: "f64", value: 1.5 }, [{ type: "f64", value: 100 }, { type: "f64", value: 1.5 }]],
                         [{type: "f64", value: -1.5 }, [{ type: "f64", value: 100 }, { type: "f64", value: -1.5 }]],
                         [{type: "f64", value: -100 }, [{ type: "f64", value: -100 }, { type: "f64", value: 1.5 }]]
                        ]
                       );
