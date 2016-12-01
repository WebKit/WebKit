import Builder from '../Builder.js'

const b = new Builder();
b.setChecked(false);
b.Type().End()
    .Function().End()
    .Code()

    .Function({ params: ["i64"], ret: "f32" }, [])
    .GetLocal(0)
    .F32ConvertUI64()
    .End()

    .Function({ params: ["i64"], ret: "f32" }, [])
    .GetLocal(0)
    .F32ConvertSI64()
    .End()

    .Function({ params: ["i32"], ret: "f32" }, [])
    .GetLocal(0)
    .F32ConvertUI32()
    .End()

    .Function({ params: ["i32"], ret: "f32" }, [])
    .GetLocal(0)
    .F32ConvertSI32()
    .End()

    .Function({ params: ["i64"], ret: "f64" }, [])
    .GetLocal(0)
    .F64ConvertUI64()
    .End()

    .Function({ params: ["i64"], ret: "f64" }, [])
    .GetLocal(0)
    .F64ConvertSI64()
    .End()

    .Function({ params: ["i32"], ret: "f64" }, [])
    .GetLocal(0)
    .F64ConvertUI32()
    .End()

    .Function({ params: ["i32"], ret: "f64" }, [])
    .GetLocal(0)
    .F64ConvertSI32()
    .End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 8,
                        [[{ type: "f32", value: 1.0 }, [{ type: "i64", value: "1" }]],
                         [{ type: "f32", value: 0.0 }, [{ type: "i64", value: "0" }]],
                         [{ type: "f32", value: 9223372036854775807 }, [{type: "i64", value: "9223372036854775807" }]],
                         [{ type: "f32", value: 9223372036854775808 }, [{type: "i64", value: "-9223372036854775808" }]],
                         [{ type: "f32", value: 18446744073709551616.0 }, [{ type: "i64", value: "0xffffffffffffffff" }]],
                         // Test rounding directions.
                         [{ type: "f32", value: 16777216.0 }, [{ type: "i64", value: "16777217" }]],
                         [{ type: "f32", value: 16777220.0 }, [{ type: "i64", value: "16777219" }]]
                        ],

                        [[{ type: "f32", value: 1.0 }, [{ type: "i64", value: "1" }]],
                         [{ type: "f32", value: -1.0}, [{ type: "i64", value: "-1" }]],
                         [{ type: "f32", value: 0.0}, [{ type: "i64", value: "0" }]],
                         [{ type: "f32", value: 9223372036854775807}, [{ type: "i64", value: "9223372036854775807" }]],
                         [{ type: "f32", value: -9223372036854775808}, [{ type: "i64", value: "-9223372036854775808" }]],
                         [{ type: "f32", value: 314159275180032.0}, [{ type: "i64", value: "314159265358979" }]],
                         // Test rounding directions.
                         [{ type: "f32", value: 16777216.0}, [{ type: "i64", value: "16777217" }]],
                         [{ type: "f32", value: -16777216.0}, [{ type: "i64", value: "-16777217" }]],
                         [{ type: "f32", value: 16777220.0}, [{ type: "i64", value: "16777219" }]],
                         [{ type: "f32", value: -16777220.0}, [{ type: "i64", value: "-16777219" }]]
                        ],

                        [[{ type: "f32", value: 1.0 }, [{ type: "i32", value: 1 }]],
                         [{ type: "f32", value: 0.0 }, [{ type: "i32", value: 0 }]],
                         [{ type: "f32", value: 2147483648 }, [{ type: "i32", value: 2147483647 }]],
                         [{ type: "f32", value: 2147483648 }, [{ type: "i32", value: -2147483648 }]],
                         [{ type: "f32", value: 305419904.0 }, [{ type: "i32", value: 0x12345678 }]],
                         [{ type: "f32", value: 4294967296.0 }, [{ type: "i32", value: -1 }]],
                         // Test rounding directions.
                         [{ type: "f32", value: 16777220.0 }, [{ type: "i32", value: 16777219 }]]
                        ],

                        [[{ type: "f32", value: 1.0 }, [{ type: "i32", value: 1 }]],
                         [{ type: "f32", value: -1.0 }, [{ type: "i32", value: -1 }]],
                         [{ type: "f32", value: 0.0 }, [{ type: "i32", value: 0 }]],
                         [{ type: "f32", value: 2147483648 }, [{ type: "i32", value: 2147483647 }]],
                         [{ type: "f32", value: -2147483648 }, [{ type: "i32", value: -2147483648 }]],
                         [{ type: "f32", value: 1234567936.0 }, [{ type: "i32", value: 1234567890 }]],
                         // Test rounding directions.
                         [{ type: "f32", value: 16777216.0 }, [{ type: "i32", value: 16777217 }]],
                         [{ type: "f32", value: -16777216.0 }, [{ type: "i32", value: -16777217 }]],
                         [{ type: "f32", value: 16777220.0 }, [{ type: "i32", value: 16777219 }]],
                         [{ type: "f32", value: -16777220.0 }, [{ type: "i32", value: -16777219 }]]
                        ],

                        [[{ type: "f64", value: 1.0 }, [{ type: "i64", value: "1" }]],
                         [{ type: "f64", value: 0.0 }, [{ type: "i64", value: "0" }]],
                         [{ type: "f64", value: 9223372036854775807 }, [{ type: "i64", value: "9223372036854775807" }]],
                         [{ type: "f64", value: 9223372036854775808 }, [{ type: "i64", value: "-9223372036854775808" }]],
                         [{ type: "f64", value: 18446744073709551616.0 }, [{ type: "i64", value: "0xffffffffffffffff" }]],
                         // Test rounding directions.
                         [{ type: "f64", value: 9007199254740992 }, [{ type: "i64", value: "9007199254740993" }]],
                         [{ type: "f64", value: 9007199254740996 }, [{ type: "i64", value: "9007199254740995" }]]
                        ],

                        [[{ type: "f64", value: 1.0 }, [{ type: "i64", value: "1" }]],
                         [{ type: "f64", value: -1.0 }, [{ type: "i64", value: "-1" }]],
                         [{ type: "f64", value: 0.0 }, [{ type: "i64", value: "0" }]],
                         [{ type: "f64", value: 9223372036854775807 }, [{ type: "i64", value: "9223372036854775807" }]],
                         [{ type: "f64", value: -9223372036854775808 }, [{ type: "i64", value: "-9223372036854775808" }]],
                         [{ type: "f64", value: 4669201609102990 }, [{ type: "i64", value: "4669201609102990" }]],
                         // Test rounding directions.
                         [{ type: "f64", value: 9007199254740992 }, [{ type: "i64", value: "9007199254740993" }]],
                         [{ type: "f64", value: -9007199254740992 }, [{ type: "i64", value: "-9007199254740993" }]],
                         [{ type: "f64", value: 9007199254740996 }, [{ type: "i64", value: "9007199254740995" }]],
                         [{ type: "f64", value: -9007199254740996 }, [{ type: "i64", value: "-9007199254740995" }]]
                        ],

                        [[{ type: "f64", value: 1.0 }, [{ type: "i32", value: 1 }]],
                         [{ type: "f64", value: 0.0 }, [{ type: "i32", value: 0 }]],
                         [{ type: "f64", value: 2147483647 }, [{ type: "i32", value: 2147483647 }]],
                         [{ type: "f64", value: 2147483648 }, [{ type: "i32", value: -2147483648 }]],
                         [{ type: "f64", value: 4294967295.0 }, [{ type: "i32", value: -1 }]]
                        ],

                        [[{ type: "f64", value: 1.0 }, [{ type: "i32", value: 1 }]],
                         [{ type: "f64", value: -1.0 }, [{ type: "i32", value: -1 }]],
                         [{ type: "f64", value: 0.0 }, [{ type: "i32", value: 0 }]],
                         [{ type: "f64", value: 2147483647 }, [{ type: "i32", value: 2147483647 }]],
                         [{ type: "f64", value: -2147483648 }, [{ type: "i32", value: -2147483648 }]],
                         [{ type: "f64", value: 987654321 }, [{ type: "i32", value: 987654321 }]]
                        ]
                       );
