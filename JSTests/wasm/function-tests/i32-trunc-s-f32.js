import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["f32"], ret: "i32" }, [])
    .GetLocal(0)
    .I32TruncSF32()
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 1,
                        [[{ type: "i32", value: "0" }, [{ type: "f32", value: "0.0" }]],
                         [{ type: "i32", value: "0" }, [{ type: "f32", value: "-0.0" }]],
                         [{ type: "i32", value: "0" }, [{ type: "f32", value: "0x1p-149" }]],
                         [{ type: "i32", value: "0" }, [{ type: "f32", value: "-0x1p-149" }]],
                         [{ type: "i32", value: "1" }, [{ type: "f32", value: "1.0" }]],
                         [{ type: "i32", value: "1" }, [{ type: "f32", value: "0x1.19999ap+0" }]],
                         [{ type: "i32", value: "1" }, [{ type: "f32", value: "1.5" }]],
                         [{ type: "i32", value: "-1" }, [{ type: "f32", value: "-1.0" }]],
                         [{ type: "i32", value: "-1" }, [{ type: "f32", value: "-0x1.19999ap+0" }]],
                         [{ type: "i32", value: "-1" }, [{ type: "f32", value: "-1.5" }]],
                         [{ type: "i32", value: "-1" }, [{ type: "f32", value: "-1.9" }]],
                         [{ type: "i32", value: "-2" }, [{ type: "f32", value: "-2.0" }]],
                         [{ type: "i32", value: "2147483520" }, [{ type: "f32", value: "2147483520.0" }]],
                         [{ type: "i32", value: "-2147483648" }, [{ type: "f32", value: "-2147483648.0" }]],
                        ],

                       );
