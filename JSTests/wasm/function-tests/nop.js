import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()

    // This function adds one to the argument.
    .Function({ params: ["i32"], ret: "i32" }, [])
    .GetLocal(0)
    .Nop()
    .End()

    // This is the id function.
    .Function({ params: ["i32"], ret: "i32" }, [])
    .Nop()
    .GetLocal(0)
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 2,
                        [[{ type: "i32", value: 0 }, [{ type: "i32", value: 0 }]],
                         [{ type: "i32", value: 1 }, [{ type: "i32", value: 1 }]],
                         [{ type: "i32", value: 100 }, [{ type: "i32", value: 100 }]],
                         [{ type: "i32", value: -100 }, [{ type: "i32", value: -100 }]],
                        ],

                        [[{ type: "i32", value: 0 }, [{ type: "i32", value: 0 }]],
                         [{ type: "i32", value: 1 }, [{ type: "i32", value: 1 }]],
                         [{ type: "i32", value: 100 }, [{ type: "i32", value: 100 }]],
                         [{ type: "i32", value: -100 }, [{ type: "i32", value: -100 }]],
                        ],
                       );
