//@ requireOptions("--useWasmWideArithmetic=1")
import * as assert from '../assert.js';

// Tests that wide arithmetic instructions in unreachable code (after return)
// don't cause compilation failures. Before the fix, these would hit the default
// case in parseUnreachableExpression() producing "invalid extended 0xfc op".
//
// Each function returns (42, 0) via return, then has a wide arithmetic op
// in unreachable code that must be parsed but not executed.

const bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,   // magic
    0x01, 0x00, 0x00, 0x00,   // version

    // type section (6 bytes)
    0x01, 0x06,
    0x01,                              // 1 type
    0x60, 0x00, 0x02, 0x7e, 0x7e,      // type 0: () -> (i64, i64)

    // function section (5 bytes)
    0x03, 0x05,
    0x04,                              // 4 functions
    0x00, 0x00, 0x00, 0x00,            // all type 0

    // export section (21 bytes)
    0x07, 0x15,
    0x04,                              // 4 exports
    0x02, 0x66, 0x30, 0x00, 0x00,      // "f0" -> func 0
    0x02, 0x66, 0x31, 0x00, 0x01,      // "f1" -> func 1
    0x02, 0x66, 0x32, 0x00, 0x02,      // "f2" -> func 2
    0x02, 0x66, 0x33, 0x00, 0x03,      // "f3" -> func 3

    // code section (41 bytes)
    0x0a, 0x29,
    0x04,                     // 4 functions

    // func 0: return then i64.add128
    0x09,                     // body length = 9
    0x00,                     // no locals
    0x42, 0x2a,               // i64.const 42
    0x42, 0x00,               // i64.const 0
    0x0f,                     // return
    0xfc, 0x13,               // i64.add128 (unreachable)
    0x0b,                     // end

    // func 1: return then i64.sub128
    0x09,                     // body length = 9
    0x00,                     // no locals
    0x42, 0x2a,               // i64.const 42
    0x42, 0x00,               // i64.const 0
    0x0f,                     // return
    0xfc, 0x14,               // i64.sub128 (unreachable)
    0x0b,                     // end

    // func 2: return then i64.mul_wide_s
    0x09,                     // body length = 9
    0x00,                     // no locals
    0x42, 0x2a,               // i64.const 42
    0x42, 0x00,               // i64.const 0
    0x0f,                     // return
    0xfc, 0x15,               // i64.mul_wide_s (unreachable)
    0x0b,                     // end

    // func 3: return then i64.mul_wide_u
    0x09,                     // body length = 9
    0x00,                     // no locals
    0x42, 0x2a,               // i64.const 42
    0x42, 0x00,               // i64.const 0
    0x0f,                     // return
    0xfc, 0x16,               // i64.mul_wide_u (unreachable)
    0x0b,                     // end
]);

function test() {
    const module = new WebAssembly.Module(bytes);
    const instance = new WebAssembly.Instance(module);
    const { f0, f1, f2, f3 } = instance.exports;

    let r;

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        r = f0();
        assert.eq(r[0], 42n);
        assert.eq(r[1], 0n);

        r = f1();
        assert.eq(r[0], 42n);
        assert.eq(r[1], 0n);

        r = f2();
        assert.eq(r[0], 42n);
        assert.eq(r[1], 0n);

        r = f3();
        assert.eq(r[0], 42n);
        assert.eq(r[1], 0n);
    }
}
noInline(test);

test();
