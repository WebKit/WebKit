import * as assert from "../assert.js";

/*
    (module
        (type (func (result i32)))
        (type (func))

        (tag (type 1))

        (table 1 funcref)
        (elem (table 0) (i32.const 0) funcref (ref.func 1))

        (func (export "test") (result i32)
            try
                i32.const 0
                ;; Should throw and jump to the outer catch block (with `i32.const 10`)
                call_indirect 0 (type 0)
                try
                    i32.const 30
                    return
                catch 0
                    ;; Should never be executed, as the contents of the inner try block cannot throw.
                    i32.const 40
                    return
                end
                drop
            catch 0
                i32.const 10
                return
            end
            i32.const 20
        )
        (func (result i32)
            throw 0
        )
    )
*/
let wasm = Uint8Array.fromBase64("AGFzbQEAAAABCAJgAAF/YAAAAwMCAAAEBAFwAAENAwEAAQcIAQR0ZXN0AAAJCwEGAEEAC3AB0gELCiQCHQAGQEEAEQAABkBBHg8HAEEoDwsaBwBBCg8LQRQLBAAIAAs=");

async function test() {
    const { instance } = await WebAssembly.instantiate(wasm);
    const { test } = instance.exports;
    for (let i = 0; i < wasmTestLoopCount; i++)
        assert.eq(test(), 10);
}

await assert.asyncTest(test());