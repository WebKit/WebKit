//@ requireOptions("--useWasmWideArithmetic=1")
import * as assert from '../assert.js';

// Tests that wide arithmetic instructions work when operands come from i64.const
// rather than local.get. This exercises the BBQ JIT's materializeToGPR path for
// const values.
//
// Functions:
//   f0: add128 all const          () -> (i64, i64)
//   f1: sub128 all const          () -> (i64, i64)
//   f2: mul_wide_u all const      () -> (i64, i64)
//   f3: mul_wide_s all const      () -> (i64, i64)
//   f4: add128 (p0, c0, p1, c0)  (i64, i64) -> (i64, i64)
//   f5: sub128 (p0, c0, p1, c0)  (i64, i64) -> (i64, i64)
//   f6: add128 (c1, p0, c0, p1)  (i64, i64) -> (i64, i64)
//   f7: mul_wide_u (p0, c3)      (i64) -> (i64, i64)
//   f8: mul_wide_s (c-1, p0)     (i64) -> (i64, i64)

const bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,   // magic
    0x01, 0x00, 0x00, 0x00,   // version

    // type section (19 bytes)
    0x01, 0x13,
    0x03,                                      // 3 types
    0x60, 0x00, 0x02, 0x7e, 0x7e,              // type 0: () -> (i64, i64)
    0x60, 0x02, 0x7e, 0x7e, 0x02, 0x7e, 0x7e,  // type 1: (i64, i64) -> (i64, i64)
    0x60, 0x01, 0x7e, 0x02, 0x7e, 0x7e,        // type 2: (i64) -> (i64, i64)

    // function section (10 bytes)
    0x03, 0x0a,
    0x09,                                                // 9 functions
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02,

    // export section (46 bytes)
    0x07, 0x2e,
    0x09,                                          // 9 exports
    0x02, 0x66, 0x30, 0x00, 0x00,                  // "f0" -> func 0
    0x02, 0x66, 0x31, 0x00, 0x01,                  // "f1" -> func 1
    0x02, 0x66, 0x32, 0x00, 0x02,                  // "f2" -> func 2
    0x02, 0x66, 0x33, 0x00, 0x03,                  // "f3" -> func 3
    0x02, 0x66, 0x34, 0x00, 0x04,                  // "f4" -> func 4
    0x02, 0x66, 0x35, 0x00, 0x05,                  // "f5" -> func 5
    0x02, 0x66, 0x36, 0x00, 0x06,                  // "f6" -> func 6
    0x02, 0x66, 0x37, 0x00, 0x07,                  // "f7" -> func 7
    0x02, 0x66, 0x38, 0x00, 0x08,                  // "f8" -> func 8

    // code section (102 bytes)
    0x0a, 0x66,
    0x09,                     // 9 functions

    // func 0: add128 all const — (5, 0) + (3, 0) = (8, 0)
    0x0c,                     // body length = 12
    0x00,                     // no locals
    0x42, 0x05,               // i64.const 5  (lhsLo)
    0x42, 0x00,               // i64.const 0  (lhsHi)
    0x42, 0x03,               // i64.const 3  (rhsLo)
    0x42, 0x00,               // i64.const 0  (rhsHi)
    0xfc, 0x13,               // i64.add128
    0x0b,                     // end

    // func 1: sub128 all const — (10, 1) - (3, 0) = (7, 1)
    0x0c,                     // body length = 12
    0x00,                     // no locals
    0x42, 0x0a,               // i64.const 10 (lhsLo)
    0x42, 0x01,               // i64.const 1  (lhsHi)
    0x42, 0x03,               // i64.const 3  (rhsLo)
    0x42, 0x00,               // i64.const 0  (rhsHi)
    0xfc, 0x14,               // i64.sub128
    0x0b,                     // end

    // func 2: mul_wide_u all const — 5 * 3 = (15, 0)
    0x08,                     // body length = 8
    0x00,                     // no locals
    0x42, 0x05,               // i64.const 5
    0x42, 0x03,               // i64.const 3
    0xfc, 0x16,               // i64.mul_wide_u
    0x0b,                     // end

    // func 3: mul_wide_s all const — (-2) * 3 = (-6, -1)
    0x08,                     // body length = 8
    0x00,                     // no locals
    0x42, 0x7e,               // i64.const -2
    0x42, 0x03,               // i64.const 3
    0xfc, 0x15,               // i64.mul_wide_s
    0x0b,                     // end

    // func 4: add128 mixed — (param0, const 0, param1, const 0)
    0x0c,                     // body length = 12
    0x00,                     // no locals
    0x20, 0x00,               // local.get 0  (lhsLo)
    0x42, 0x00,               // i64.const 0  (lhsHi)
    0x20, 0x01,               // local.get 1  (rhsLo)
    0x42, 0x00,               // i64.const 0  (rhsHi)
    0xfc, 0x13,               // i64.add128
    0x0b,                     // end

    // func 5: sub128 mixed — (param0, const 0, param1, const 0)
    0x0c,                     // body length = 12
    0x00,                     // no locals
    0x20, 0x00,               // local.get 0  (lhsLo)
    0x42, 0x00,               // i64.const 0  (lhsHi)
    0x20, 0x01,               // local.get 1  (rhsLo)
    0x42, 0x00,               // i64.const 0  (rhsHi)
    0xfc, 0x14,               // i64.sub128
    0x0b,                     // end

    // func 6: add128 mixed — (const 1, param0, const 0, param1)
    0x0c,                     // body length = 12
    0x00,                     // no locals
    0x42, 0x01,               // i64.const 1  (lhsLo)
    0x20, 0x00,               // local.get 0  (lhsHi)
    0x42, 0x00,               // i64.const 0  (rhsLo)
    0x20, 0x01,               // local.get 1  (rhsHi)
    0xfc, 0x13,               // i64.add128
    0x0b,                     // end

    // func 7: mul_wide_u mixed — (param0, const 3)
    0x08,                     // body length = 8
    0x00,                     // no locals
    0x20, 0x00,               // local.get 0
    0x42, 0x03,               // i64.const 3
    0xfc, 0x16,               // i64.mul_wide_u
    0x0b,                     // end

    // func 8: mul_wide_s mixed — (const -1, param0)
    0x08,                     // body length = 8
    0x00,                     // no locals
    0x42, 0x7f,               // i64.const -1
    0x20, 0x00,               // local.get 0
    0xfc, 0x15,               // i64.mul_wide_s
    0x0b,                     // end
]);

function test() {
    const module = new WebAssembly.Module(bytes);
    const instance = new WebAssembly.Instance(module);
    const { f0, f1, f2, f3, f4, f5, f6, f7, f8 } = instance.exports;

    let r;

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        // All-const tests
        r = f0();
        assert.eq(r[0], 8n);
        assert.eq(r[1], 0n);

        r = f1();
        assert.eq(r[0], 7n);
        assert.eq(r[1], 1n);

        r = f2();
        assert.eq(r[0], 15n);
        assert.eq(r[1], 0n);

        r = f3();
        assert.eq(r[0], -6n);
        assert.eq(r[1], -1n);

        // Mixed add128: (param0, 0) + (param1, 0)
        r = f4(5n, 3n);
        assert.eq(r[0], 8n);
        assert.eq(r[1], 0n);

        r = f4(-1n, 1n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 1n);

        // Mixed sub128: (param0, 0) - (param1, 0)
        r = f5(10n, 3n);
        assert.eq(r[0], 7n);
        assert.eq(r[1], 0n);

        r = f5(1n, 3n);
        assert.eq(r[0], -2n);
        assert.eq(r[1], -1n);

        // Mixed add128: (1, param0) + (0, param1)
        r = f6(2n, 3n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], 5n);

        r = f6(0n, -1n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], -1n);

        // Mixed mul_wide_u: param0 * 3
        r = f7(5n);
        assert.eq(r[0], 15n);
        assert.eq(r[1], 0n);

        r = f7(-1n);
        assert.eq(r[0], -3n);
        assert.eq(r[1], 2n);

        // Mixed mul_wide_s: (-1) * param0
        r = f8(5n);
        assert.eq(r[0], -5n);
        assert.eq(r[1], -1n);

        r = f8(-1n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], 0n);
    }
}
noInline(test);

test();
