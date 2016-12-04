import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["i32"], ret: "i32" }, [])
    .GetLocal(0)
    .I32Ctz()
    .End()

    .Function({ params: ["i64"], ret: "i64" }, [])
    .GetLocal(0)
    .I64Ctz()
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 2,
                        [[{type: "i32", value: "0" }, [{ type: "i32", value: "-1" }]],
                         [{type: "i32", value: "32" }, [{ type: "i32", value: "0" }]],
                         [{type: "i32", value: "15" }, [{ type: "i32", value: "0x00008000" }]],
                         [{type: "i32", value: "16" }, [{ type: "i32", value: "0x00010000" }]],
                         [{type: "i32", value: "31" }, [{ type: "i32", value: "0x80000000" }]],
                         [{type: "i32", value: "0" }, [{ type: "i32", value: "0x7fffffff" }]],
                        ],

                        [[{type: "i64", value: "0" }, [{ type: "i64", value: "-1" }]],
                         [{type: "i64", value: "64" }, [{ type: "i64", value: "0" }]],
                         [{type: "i64", value: "15" }, [{ type: "i64", value: "0x00008000" }]],
                         [{type: "i64", value: "16" }, [{ type: "i64", value: "0x00010000" }]],
                         [{type: "i64", value: "63" }, [{ type: "i64", value: "0x8000000000000000" }]],
                         [{type: "i64", value: "0" }, [{ type: "i64", value: "0x7fffffffffffffff" }]],
                         ]
                       );
