import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()

    // This function multiplies the argument by 2
    .Function({ params: ["i32"], ret: "i32" }, [])
    .GetLocal(0)
    .I32Const(1)
    .TeeLocal(0)
    .GetLocal(0)
    .I32Add()
    .I32Mul()
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 1,
                        [[{ type: "i32", value: 0 }, [{ type: "i32", value: 0 }]],
                         [{ type: "i32", value: 2 }, [{ type: "i32", value: 1 }]],
                         [{ type: "i32", value: 200 }, [{ type: "i32", value: 100 }]],
                         [{ type: "i32", value: -200 }, [{ type: "i32", value: -100 }]],
                        ],
                       );
