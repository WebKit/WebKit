import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()

    .Function({ params: ["i32"], ret: "i32" })
    .I32Const(1)
    .End()

    .Function({ params: ["i32"], ret: "i32" })
    .GetLocal(0)
    .End()

    .Function({ params: ["i32", "i32"], ret: "i32" })
    .GetLocal(1)
    .GetLocal(0)
    .CallIndirect(0, 0)
    .End()


const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 3, [], [],
                        [[{ type: "i32", value: 1 }, [{ type: "i32", value: 0 }, { type: "i32", value: 4 }]],
                         [{ type: "i32", value: 4 }, [{ type: "i32", value: 1 }, { type: "i32", value: 4 }]],
                        ]);
