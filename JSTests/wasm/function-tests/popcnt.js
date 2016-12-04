import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["i32"], ret: "i32" }, [])
    .GetLocal(0)
    .I32Popcnt()
    .End()

    .Function({ params: ["i64"], ret: "i64" }, [])
    .GetLocal(0)
    .I64Popcnt()
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 2,
                        [[{type: "i32", value: "32" }, [{ type: "i32", value: "-1" }]],
                         [{type: "i32", value: "0" }, [{ type: "i32", value: "0" }]],
                         [{type: "i32", value: "1" }, [{ type: "i32", value: "0x00008000" }]],
                         [{type: "i32", value: "2" }, [{ type: "i32", value: "0x80008000" }]],
                         [{type: "i32", value: "31" }, [{ type: "i32", value: "0x7fffffff" }]],
                         [{type: "i32", value: "16" }, [{ type: "i32", value: "0xaaaaaaaa" }]],
                         [{type: "i32", value: "16" }, [{ type: "i32", value: "0x55555555" }]],
                         [{type: "i32", value: "24" }, [{ type: "i32", value: "0xdeadbeef" }]],
                        ],


                        [[{type: "i64", value: "64" }, [{ type: "i64", value: "-1" }]],
                         [{type: "i64", value: "0" }, [{ type: "i64", value: "0" }]],
                         [{type: "i64", value: "1" }, [{ type: "i64", value: "0x00008000" }]],
                         [{type: "i64", value: "4" }, [{ type: "i64", value: "0x8000800080008000" }]],
                         [{type: "i64", value: "63" }, [{ type: "i64", value: "0x7fffffffffffffff" }]],
                         [{type: "i64", value: "32" }, [{ type: "i64", value: "0xaaaaaaaa55555555" }]],
                         [{type: "i64", value: "32" }, [{ type: "i64", value: "0x99999999aaaaaaaa" }]],
                         [{type: "i64", value: "48" }, [{ type: "i64", value: "0xdeadbeefdeadbeef" }]],
                        ]
                       );
