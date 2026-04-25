function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

{
    // (module
    //  (func $bar (param $x i32) (result (ref i31))
    //        (ref.i31 (local.get $x)))
    //  (func $foo (export "foo") (param $x i32) (result (ref null i31))
    //        (if (result (ref null i31))
    //            (i32.eqz (local.get $x))
    //            (then (ref.null i31))
    //            (else (return_call $bar (local.get $x))))))
    const wasmCode = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 12, 2, 96, 1, 127, 1, 100, 108, 96, 1, 127, 1, 108, 3, 3, 2, 0, 1, 7, 7, 1, 3, 102, 111, 111, 0, 1, 10, 24, 2, 6, 0, 32, 0, 251, 28, 11, 15, 0, 32, 0, 69, 4, 108, 208, 108, 5, 32, 0, 18, 0, 11, 11, 0, 31, 4, 110, 97, 109, 101, 1, 11, 2, 0, 3, 98, 97, 114, 1, 3, 102, 111, 111, 2, 11, 2, 0, 1, 0, 1, 120, 1, 1, 0, 1, 120]);
    const wasmModule = new WebAssembly.Module(wasmCode);
    const wasmInstance = new WebAssembly.Instance(wasmModule);

    shouldBe(wasmInstance.exports.foo(42), 42);
}

{
    // (module
    //   (table 2 funcref)
    //   (elem (i32.const 0) $bar)
    //   (type $i32_to_ref_i31 (func (param i32) (result (ref i31))))
    //   (func $bar (param $x i32) (result (ref i31))
    //     (ref.i31 (local.get $x)))
    //   (func $foo (export "foo") (param $x i32) (result (ref null i31))
    //     (if (result (ref null i31))
    //       (i32.eqz (local.get $x))
    //       (then (ref.null i31))
    //       (else (return_call_indirect (type $i32_to_ref_i31)
    //                                   (local.get $x)
    //                                   (i32.const 0))))))
    const wasmCode = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 12, 2, 96, 1, 127, 1, 100, 108, 96, 1, 127, 1, 108, 3, 3, 2, 0, 1, 4, 4, 1, 112, 0, 2, 7, 7, 1, 3, 102, 111, 111, 0, 1, 9, 7, 1, 0, 65, 0, 11, 1, 0, 10, 27, 2, 6, 0, 32, 0, 251, 28, 11, 18, 0, 32, 0, 69, 4, 108, 208, 108, 5, 32, 0, 65, 0, 19, 0, 0, 11, 11, 0, 50, 4, 110, 97, 109, 101, 1, 11, 2, 0, 3, 98, 97, 114, 1, 3, 102, 111, 111, 2, 11, 2, 0, 1, 0, 1, 120, 1, 1, 0, 1, 120, 4, 17, 1, 0, 14, 105, 51, 50, 95, 116, 111, 95, 114, 101, 102, 95, 105, 51, 49]);
    const wasmModule = new WebAssembly.Module(wasmCode);
    const wasmInstance = new WebAssembly.Instance(wasmModule);

    shouldBe(wasmInstance.exports.foo(42), 42);
}
