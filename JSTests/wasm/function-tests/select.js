import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["i32"], ret: "i32" }, [])
    .I32Const(1)
    .I32Const(2)
    .GetLocal(0)
    .Select()
    .End()

// This function should be foo(x, y) { return x ? x : y; }
    .Function({ params: ["i32", "i32"], ret: "i32" }, [])
    .GetLocal(0)
    .GetLocal(1)
    .GetLocal(0)
    .Select()
    .End()

const bin = b.WebAssembly()
bin.trim()
testWasmModuleFunctions(bin.get(), 2, [
    [{ type: "i32", value: 2 }, [{ type: "i32", value: 0 }]],
    [{ type: "i32", value: 1 }, [{ type: "i32", value: 1 }]],
    [{ type: "i32", value: 1 }, [{ type: "i32", value: -1 }]]
], [
    [{ type: "i32", value: 1 }, [{ type: "i32", value: 0 }, { type: "i32", value: 1 }]],
    [{ type: "i32", value: -1 }, [{ type: "i32", value: 0 }, { type: "i32", value: -1 }]],
    [{ type: "i32", value: 1 }, [{ type: "i32", value: 1 }, { type: "i32", value: 10 }]],
    [{ type: "i32", value: 30 }, [{ type: "i32", value: 30 }, { type: "i32", value: 10 }]]
]);
