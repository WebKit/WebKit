import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["f32"], ret: "i64" }, [])
    .GetLocal(0)
    .I64TruncUF32()
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
                         [{ type: "i64", value: "4294967296" }, [{ type: "f32", value: "4294967296" }]],
                         [{ type: "i64", value: "-1099511627776" }, [{ type: "f32", value: "18446742974197923840.0" }]],
                         [{ type: "i64", value: "0" }, [{ type: "f32", value: "-0x1.ccccccp-1" }]],
                         [{ type: "i64", value: "0" }, [{ type: "f32", value: "-0x1.fffffep-1" }]],[{ type: "i64", value: "0" }, [{ type: "f32", value: "0.0" }]],
                        ],
                       );
