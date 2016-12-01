import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["f32", "f32"], ret: "f32" }, [])
    .GetLocal(0)
    .GetLocal(1)
    .F32Sub()
    .Return()
    .End()

    .Function({ params: ["f32", "f32"], ret: "f32" }, [])
    .GetLocal(0)
    .GetLocal(1)
    .Call(0)
    .Return()
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 2,
                        [[{type: "f32", value: -1.5 }, [{ type: "f32", value: 0 }, { type: "f32", value: 1.5 }]],
                         [{type: "f32", value: 87.6234 }, [{ type: "f32", value: 100.1234 }, { type: "f32", value: 12.5 }]]
                        ],
                        [[{type: "f32", value: -1.5 }, [{ type: "f32", value: 0 }, { type: "f32", value: 1.5 }]],
                         [{type: "f32", value: 87.6234 }, [{ type: "f32", value: 100.1234 }, { type: "f32", value: 12.5 }]]
                        ]
                       );
