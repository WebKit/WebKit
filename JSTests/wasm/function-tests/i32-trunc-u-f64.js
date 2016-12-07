import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["f64"], ret: "i32" }, [])
    .GetLocal(0)
    .I32TruncUF64()
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 1,
                        [[{ type: "i32", value: "0" }, [{ type: "f64", value: "0.0" }]],
                         [{ type: "i32", value: "0" }, [{ type: "f64", value: "-0.0" }]],
                         [{ type: "i32", value: "0" }, [{ type: "f64", value: "0x0.0000000000001p-1022" }]],
                         [{ type: "i32", value: "0" }, [{ type: "f64", value: "-0x0.0000000000001p-1022" }]],
                         [{ type: "i32", value: "1" }, [{ type: "f64", value: "1.0" }]],
                         [{ type: "i32", value: "1" }, [{ type: "f64", value: "0x1.199999999999ap+0" }]],
                         [{ type: "i32", value: "1" }, [{ type: "f64", value: "1.5" }]],
                         [{ type: "i32", value: "1" }, [{ type: "f64", value: "1.9" }]],
                         [{ type: "i32", value: "2" }, [{ type: "f64", value: "2.0" }]],
                         [{ type: "i32", value: "-2147483648" }, [{ type: "f64", value: "2147483648" }]],
                         [{ type: "i32", value: "-1" }, [{ type: "f64", value: "4294967295.0" }]],
                         [{ type: "i32", value: "0" }, [{ type: "f64", value: "-0x1.ccccccccccccdp-1" }]],
                         [{ type: "i32", value: "0" }, [{ type: "f64", value: "-0x1.fffffffffffffp-1" }]],
                         [{ type: "i32", value: "100000000" }, [{ type: "f64", value: "1e8" }]],
                        ],
                       );
