/*
    (module
        (func (export "funcRefFuncRef") (param $p0 f32) (param $p1 funcref) (result funcref funcref)
            (local.get $p1)
            (local.get $p1)
            (return)
        )

        (func (export "intInt") (param $p0 f32) (param $p1 funcref) (result i32 i32)
            (i32.const 42)
            (i32.const 43)
            (return)
        )

        (func (export "funcRefInt") (param $p0 f32) (param $p1 funcref) (result funcref i32)
            (local.get $p1)
            (i32.const 42)
            (return)
        )

        (func (export "intFuncRef1") (param $p0 f32) (param $p1 funcref) (result i32 funcref)
            (i32.const 42)
            (local.get $p1)
            (return)
        )

        (func (export "intFuncRef2") (param $p0 f32) (param $p1 funcref) (result i64 funcref)
            (i64.const 0x141414142)
            (local.get $p1)
            (return)
        )

        (func (export "intFuncRef3") (param $p0 funcref) (param $p1 f32) (result i32 funcref)
            (i32.const 42)
            (local.get $p0)
            (return)
        )

        (func (export "intFuncRef4") (param $p0 funcref) (param $p1 i32) (result i32 funcref)
            (i32.const 42)
            (local.get $p0)
            (return)
        )

        (func (export "floatFloat1") (param $p0 f32) (param $p1 i32) (result f32 f32)
            (f32.const 42)
            (local.get $p0)
            (return)
        )

        (func (export "floatFloat2") (param $p0 i32) (param $p1 f32) (result f32 f32)
            (f32.const 42)
            (local.get $p1)
            (return)
        )
    )
*/
let wasm_code = new Uint8Array([
    0, 97, 115, 109, 1, 0, 0, 0, 1, 64, 9, 96, 2, 125, 112, 2, 112, 112, 96, 2, 125, 112, 2, 127, 127, 96, 2, 125, 112, 2, 112, 127, 96, 2, 125, 112, 2, 127, 112, 96, 2, 125, 112, 2, 126, 112, 96, 2, 112, 125, 2, 127, 112, 96, 2, 112, 127, 2, 127, 112, 96, 2, 125, 127, 2, 125, 125, 96, 2, 127, 125, 2, 125, 125, 3, 10, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 7, 124, 9, 14, 102, 117, 110, 99, 82, 101, 102, 70, 117, 110, 99, 82, 101, 102, 0, 0, 6, 105, 110, 116, 73, 110, 116, 0, 1, 10, 102, 117, 110, 99, 82, 101, 102, 73, 110, 116, 0, 2, 11, 105, 110, 116, 70, 117, 110, 99, 82, 101, 102, 49, 0, 3, 11, 105, 110, 116, 70, 117, 110, 99, 82, 101, 102, 50, 0, 4, 11, 105, 110, 116, 70, 117, 110, 99, 82, 101, 102, 51, 0, 5, 11, 105, 110, 116, 70, 117, 110, 99, 82, 101, 102, 52, 0, 6, 11, 102, 108, 111, 97, 116, 70, 108, 111, 97, 116, 49, 0, 7, 11, 102, 108, 111, 97, 116, 70, 108, 111, 97, 116, 50, 0, 8, 10, 83, 9, 7, 0, 32, 1, 32, 1, 15, 11, 7, 0, 65, 42, 65, 43, 15, 11, 7, 0, 32, 1, 65, 42, 15, 11, 7, 0, 65, 42, 32, 1, 15, 11, 11, 0, 66, 194, 130, 133, 138, 20, 32, 1, 15, 11, 7, 0, 65, 42, 32, 0, 15, 11, 7, 0, 65, 42, 32, 0, 15, 11, 10, 0, 67, 0, 0, 40, 66, 32, 0, 15, 11, 10, 0, 67, 0, 0, 40, 66, 32, 1, 15, 11
]);

let wasm_module = new WebAssembly.Module(wasm_code);
let wasm_instance = new WebAssembly.Instance(wasm_module);
const {
    funcRefFuncRef,
    intInt,
    funcRefInt,
    intFuncRef1,
    intFuncRef2,
    intFuncRef3,
    intFuncRef4,
    floatFloat1,
    floatFloat2,
} = wasm_instance.exports

let testTuple = (tuple, expected1, expected2) => {
    function assert(b) {
        if (!b)
            throw new Error("bad");
    }
    assert(tuple[0] === expected1);
    assert(tuple[1] === expected2);
};

testTuple(funcRefFuncRef(0, null), null, null);
testTuple(intInt(0, null), 42, 43);
testTuple(funcRefInt(0, null), null, 42);
testTuple(intFuncRef1(0, null), 42, null);
testTuple(intFuncRef2(0, null), 0x141414142n, null);
testTuple(intFuncRef3(null, null), 42, null);
testTuple(intFuncRef4(null, 0), 42, null);
testTuple(floatFloat1(0, 1), 42, 0);
testTuple(floatFloat2(1, 0), 42, 0);
