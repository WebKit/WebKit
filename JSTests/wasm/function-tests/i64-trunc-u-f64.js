import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["f64"], ret: "i64" }, [])
    .GetLocal(0)
    .I64TruncUF64()
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 1,
                        [[{ type: "i64", value: "0" }, [{ type: "f64", value: "0.0" }]],
                         [{ type: "i64", value: "0" }, [{ type: "f64", value: "-0.0" }]],
                         [{ type: "i64", value: "0" }, [{ type: "f64", value: "0x0.0000000000001p-1022" }]],
                         [{ type: "i64", value: "0" }, [{ type: "f64", value: "-0x0.0000000000001p-1022" }]],
                         [{ type: "i64", value: "1" }, [{ type: "f64", value: "1.0" }]],
                         [{ type: "i64", value: "1" }, [{ type: "f64", value: "0x1.199999999999ap+0" }]],
                         [{ type: "i64", value: "1" }, [{ type: "f64", value: "1.5" }]],
                         [{ type: "i64", value: "0xffffffff" }, [{ type: "f64", value: "4294967295" }]],
                         [{ type: "i64", value: "0x100000000" }, [{ type: "f64", value: "4294967296" }]],
                         [{ type: "i64", value: "-2048" }, [{ type: "f64", value: "18446744073709549568.0" }]],
                         [{ type: "i64", value: "0" }, [{ type: "f64", value: "-0x1.ccccccccccccdp-1" }]],
                         [{ type: "i64", value: "0" }, [{ type: "f64", value: "-0x1.fffffffffffffp-1" }]],
                         [{ type: "i64", value: "100000000" }, [{ type: "f64", value: "1e8" }]],
                         [{ type: "i64", value: "10000000000000000" }, [{ type: "f64", value: "1e16" }]],
                         [{ type: "i64", value: "-9223372036854775808" }, [{ type: "f64", value: "9223372036854775808" }]],
                        ],

                       );
