//@ requireOptions("--useWasmWideArithmetic=1", "--useBBQJIT=1", "--useOMGJIT=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeSoon=0")
import * as assert from '../assert.js';

// Test that i64.mul_wide_u and i64.mul_wide_s produce correct results in the
// BBQ JIT under register pressure. On x86_64, the mul instruction produces
// results in rdx:rax. If the register allocator assigns resultLo to rdx and
// resultHi to rax, the move sequence must not destroy the low half.
// This function sets 6 live locals before mul_wide to force the allocator
// toward using eax/edx for the results.

const bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,

    // type section: (i64, i64) -> (i64, i64)
    0x01, 0x08, 0x01,
    0x60, 0x02, 0x7e, 0x7e, 0x02, 0x7e, 0x7e,

    // function section
    0x03, 0x02, 0x01, 0x00,

    // export section: "mul_wide_u_pressure" -> func 0
    0x07, 0x17, 0x01,
    0x13, 0x6d, 0x75, 0x6c, 0x5f, 0x77, 0x69, 0x64, 0x65, 0x5f, 0x75, 0x5f, 0x70, 0x72, 0x65, 0x73, 0x73, 0x75, 0x72, 0x65,
    0x00, 0x00,

    // code section
    0x0a, 0x48, 0x01,
    0x46,                     // body size = 70
    0x01, 0x06, 0x7e,         // 1 local decl group: 6 i64 locals (indices 2-7)
    0x20, 0x00, 0x42, 0x01, 0x7c, 0x21, 0x02,   // l2 = a + 1
    0x20, 0x00, 0x42, 0x02, 0x7c, 0x21, 0x03,   // l3 = a + 2
    0x20, 0x00, 0x42, 0x03, 0x7c, 0x21, 0x04,   // l4 = a + 3
    0x20, 0x01, 0x42, 0x04, 0x7c, 0x21, 0x05,   // l5 = b + 4
    0x20, 0x01, 0x42, 0x05, 0x7c, 0x21, 0x06,   // l6 = b + 5
    0x20, 0x01, 0x42, 0x06, 0x7c, 0x21, 0x07,   // l7 = b + 6
    0x20, 0x00,               // local.get a
    0x20, 0x01,               // local.get b
    0xfc, 0x16,               // i64.mul_wide_u
    0x20, 0x02, 0x20, 0x03, 0x7c, 0x1a,        // drop(l2 + l3)
    0x20, 0x04, 0x20, 0x05, 0x7c, 0x1a,        // drop(l4 + l5)
    0x20, 0x06, 0x20, 0x07, 0x7c, 0x1a,        // drop(l6 + l7)
    0x0b,
]);

const module = new WebAssembly.Module(bytes);
const instance = new WebAssembly.Instance(module);
const mul_wide_u_pressure = instance.exports["mul_wide_u_pressure"];

// Cases where the low and high halves differ, so a swap would be detected.
function computeExpected(a, b) {
    const ua = BigInt.asUintN(64, a);
    const ub = BigInt.asUintN(64, b);
    const product = ua * ub;
    return [product & 0xFFFFFFFFFFFFFFFFn, (product >> 64n) & 0xFFFFFFFFFFFFFFFFn];
}

const cases = [
    [0xFFFFFFFFFFFFFFFFn, 0xFFFFFFFFFFFFFFFFn],
    [0x8000000000000000n, 0x8000000000000000n],
    [0x7FFFFFFFFFFFFFFFn, 0x7FFFFFFFFFFFFFFFn],
    [1234567890123456789n, 9876543210987654321n],
    [1n, 1n],
    [0n, 0n],
    [-1n, -1n],
    [-1n, 1n],
    [42n, 27n],
    [0xDEADBEEFCAFEBABEn, 0x0123456789ABCDEFn],
];

for (let i = 0; i < wasmTestLoopCount; ++i) {
    for (const [a, b] of cases) {
        const r = mul_wide_u_pressure(a, b);
        const [expectedLo, expectedHi] = computeExpected(a, b);
        assert.eq(BigInt.asUintN(64, r[0]), expectedLo);
        assert.eq(BigInt.asUintN(64, r[1]), expectedHi);
    }
}
