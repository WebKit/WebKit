import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()

    // This function adds one to the argument.
    .Function({ params: ["i32"], ret: "i32" }, [])
    .GetLocal(0)
    .I32Const(1)
    .TeeLocal(0)
    .GetLocal(0)
    .Drop()
    .I32Add()
    .End()

    // This is the id function.
    .Function({ params: ["i32"], ret: "i32" }, [])
    .GetLocal(0)
    .I32Const(1)
    .Drop()
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 2,
                        [[{ type: "i32", value: 1 }, [{ type: "i32", value: 0 }]],
                         [{ type: "i32", value: 2 }, [{ type: "i32", value: 1 }]],
                         [{ type: "i32", value: 101 }, [{ type: "i32", value: 100 }]],
                         [{ type: "i32", value: -99 }, [{ type: "i32", value: -100 }]],
                        ],

                        [[{ type: "i32", value: 0 }, [{ type: "i32", value: 0 }]],
                         [{ type: "i32", value: 1 }, [{ type: "i32", value: 1 }]],
                         [{ type: "i32", value: 100 }, [{ type: "i32", value: 100 }]],
                         [{ type: "i32", value: -100 }, [{ type: "i32", value: -100 }]],
                        ],
                       );
