import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["f32"], ret: "i64" }, [])
    .GetLocal(0)
    .I64TruncSF32()
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 1,
                        [[{ type: "i64", value: "0" }, [{ type: "f32", value: "0.0" }]],
                         [{ type: "i64", value: "0" }, [{ type: "f32", value: "-0.0" }]],
                         [{ type: "i64", value: "0" }, [{ type: "f32", value: "0x1p-149" }]],
                         [{ type: "i64", value: "0" }, [{ type: "f32", value: "-0x1p-149" }]],
                         [{ type: "i64", value: "1" }, [{ type: "f32", value: "1.0" }]],
                         [{ type: "i64", value: "1" }, [{ type: "f32", value: "0x1.19999ap+0" }]],
                         [{ type: "i64", value: "1" }, [{ type: "f32", value: "1.5" }]],
                         [{ type: "i64", value: "-1" }, [{ type: "f32", value: "-1.0" }]],
                         [{ type: "i64", value: "-1" }, [{ type: "f32", value: "-0x1.19999ap+0" }]],
                         [{ type: "i64", value: "-1" }, [{ type: "f32", value: "-1.5" }]],
                         [{ type: "i64", value: "-1" }, [{ type: "f32", value: "-1.9" }]],
                         [{ type: "i64", value: "-2" }, [{ type: "f32", value: "-2.0" }]],
                         [{ type: "i64", value: "4294967296" }, [{ type: "f32", value: "4294967296" }]],
                         [{ type: "i64", value: "-4294967296" }, [{ type: "f32", value: "-4294967296" }]],
                         [{ type: "i64", value: "9223371487098961920" }, [{ type: "f32", value: "9223371487098961920.0" }]],
                         [{ type: "i64", value: "-9223372036854775808" }, [{ type: "f32", value: "-9223372036854775808.0" }]],
                        ],

                       );
