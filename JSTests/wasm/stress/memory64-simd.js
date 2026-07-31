//@ skip if !$isSIMDPlatform
//@ skip if $addressBits <= 32

import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const pageSize = 65536n;
const initialPages = 2n;
const maximumPages = 3n;
const memorySize = initialPages * pageSize;

// 2^32. Only meaningful in a 64-bit address computation: every access using it is out
// of bounds here, so an implementation that truncated the offset would wrongly succeed.
const hugeOffset = 4294967296n;

const zeroVector = "(v128.const i64x2 0 0)";
const dataVector = "(v128.const i64x2 0x7766554433221100 0xFFEEDDCCBBAA9988)";

// Instructions reading from $src. `width` is the number of bytes touched at $src.
const loadOps = [
    { name: "v128_load",        width: 16n, body: memarg => `(v128.load ${memarg} (local.get $src))` },
    { name: "v128_load8x8_s",   width:  8n, body: memarg => `(v128.load8x8_s ${memarg} (local.get $src))` },
    { name: "v128_load8x8_u",   width:  8n, body: memarg => `(v128.load8x8_u ${memarg} (local.get $src))` },
    { name: "v128_load16x4_s",  width:  8n, body: memarg => `(v128.load16x4_s ${memarg} (local.get $src))` },
    { name: "v128_load16x4_u",  width:  8n, body: memarg => `(v128.load16x4_u ${memarg} (local.get $src))` },
    { name: "v128_load32x2_s",  width:  8n, body: memarg => `(v128.load32x2_s ${memarg} (local.get $src))` },
    { name: "v128_load32x2_u",  width:  8n, body: memarg => `(v128.load32x2_u ${memarg} (local.get $src))` },
    { name: "v128_load8_splat", width:  1n, body: memarg => `(v128.load8_splat ${memarg} (local.get $src))` },
    { name: "v128_load16_splat", width: 2n, body: memarg => `(v128.load16_splat ${memarg} (local.get $src))` },
    { name: "v128_load32_splat", width: 4n, body: memarg => `(v128.load32_splat ${memarg} (local.get $src))` },
    { name: "v128_load64_splat", width: 8n, body: memarg => `(v128.load64_splat ${memarg} (local.get $src))` },
    { name: "v128_load8_lane",  width:  1n, body: memarg => `(v128.load8_lane ${memarg} 13 (local.get $src) ${zeroVector})` },
    { name: "v128_load16_lane", width:  2n, body: memarg => `(v128.load16_lane ${memarg} 6 (local.get $src) ${zeroVector})` },
    { name: "v128_load32_lane", width:  4n, body: memarg => `(v128.load32_lane ${memarg} 3 (local.get $src) ${zeroVector})` },
    { name: "v128_load64_lane", width:  8n, body: memarg => `(v128.load64_lane ${memarg} 0 (local.get $src) ${zeroVector})` },
    { name: "v128_load32_zero", width:  4n, body: memarg => `(v128.load32_zero ${memarg} (local.get $src))` },
    { name: "v128_load64_zero", width:  8n, body: memarg => `(v128.load64_zero ${memarg} (local.get $src))` },
];

// Instructions writing to $addr. `width` is the number of bytes touched at $addr.
const storeOps = [
    { name: "v128_store",        width: 16n, body: memarg => `(v128.store ${memarg} (local.get $addr) ${dataVector})` },
    { name: "v128_store8_lane",  width:  1n, body: memarg => `(v128.store8_lane ${memarg} 8 (local.get $addr) ${dataVector})` },
    { name: "v128_store16_lane", width:  2n, body: memarg => `(v128.store16_lane ${memarg} 4 (local.get $addr) ${dataVector})` },
    { name: "v128_store32_lane", width:  4n, body: memarg => `(v128.store32_lane ${memarg} 2 (local.get $addr) ${dataVector})` },
    { name: "v128_store64_lane", width:  8n, body: memarg => `(v128.store64_lane ${memarg} 1 (local.get $addr) ${dataVector})` },
];

let wat = `
(module
    (memory i64 ${initialPages} ${maximumPages})

    (func (export "i64_store") (param $addr i64) (param $value i64)
        (i64.store (local.get $addr) (local.get $value))
    )

    (func (export "i64_load") (param $addr i64) (result i64)
        (i64.load (local.get $addr))
    )

    (func (export "grow") (param $delta i64) (result i64)
        (memory.grow (local.get $delta))
    )
    ${
      // Each load-like instruction, wrapped so the loaded vector is written back to
      // memory at $dst: v128 cannot cross the JS boundary, so results are read back
      // through i64.load.
      loadOps.map(({ name, body }) =>
        `(func (export "${name}") (param $src i64) (param $dst i64)
            (v128.store (local.get $dst) ${body("")})
        )
        (func (export "${name}_huge_offset") (param $src i64) (param $dst i64)
            (v128.store (local.get $dst) ${body(`offset=${hugeOffset}`)})
        )`).join("\n    ")
    }
    ${
      storeOps.map(({ name, body }) =>
        `(func (export "${name}") (param $addr i64)
            ${body("")}
        )
        (func (export "${name}_huge_offset") (param $addr i64)
            ${body(`offset=${hugeOffset}`)}
        )`).join("\n    ")
    }

    ;; A static offset that does not fit in 16 bits, exercised against an i64 base.
    (func (export "v128_load_page_offset") (param $src i64) (param $dst i64)
        (v128.store (local.get $dst) (v128.load offset=${pageSize} (local.get $src)))
    )

    (func (export "v128_store_page_offset") (param $addr i64)
        (v128.store offset=${pageSize} (local.get $addr) ${dataVector})
    )
)
`;

const instance = await instantiate(wat, {}, { memory64: true, simd: true });
const exports = instance.exports;
const { i64_load, i64_store, grow } = exports;

// i64 results come back sign-extended; the expectations below are written unsigned.
const asI64 = value => BigInt.asIntN(64, value);

const outOfBounds = (fn) =>
    assert.throws(fn, WebAssembly.RuntimeError, "Out of bounds memory access");

function testAt(base) {
    const src = base;
    const dst = base + 16n;

    const check = (lo, hi) => {
        assert.eq(i64_load(dst), asI64(lo));
        assert.eq(i64_load(dst + 8n), asI64(hi));
    };

    i64_store(src, 0x7766554433221100n);
    i64_store(src + 8n, 0xFFEEDDCCBBAA9988n);

    exports.v128_load(src, dst);
    check(0x7766554433221100n, 0xFFEEDDCCBBAA9988n);

    exports.v128_load8x8_s(src + 8n, dst);
    check(0xFFBBFFAAFF99FF88n, 0xFFFFFFEEFFDDFFCCn);

    exports.v128_load8x8_u(src + 8n, dst);
    check(0x00BB00AA00990088n, 0x00FF00EE00DD00CCn);

    exports.v128_load16x4_s(src + 8n, dst);
    check(0xFFFFBBAAFFFF9988n, 0xFFFFFFEEFFFFDDCCn);

    exports.v128_load16x4_u(src + 8n, dst);
    check(0x0000BBAA00009988n, 0x0000FFEE0000DDCCn);

    exports.v128_load32x2_s(src + 8n, dst);
    check(0xFFFFFFFFBBAA9988n, 0xFFFFFFFFFFEEDDCCn);

    exports.v128_load32x2_u(src + 8n, dst);
    check(0x00000000BBAA9988n, 0x00000000FFEEDDCCn);

    exports.v128_load8_splat(src + 1n, dst);
    check(0x1111111111111111n, 0x1111111111111111n);

    exports.v128_load16_splat(src, dst);
    check(0x1100110011001100n, 0x1100110011001100n);

    exports.v128_load32_splat(src, dst);
    check(0x3322110033221100n, 0x3322110033221100n);

    exports.v128_load64_splat(src, dst);
    check(0x7766554433221100n, 0x7766554433221100n);

    // Lane loads replace one lane of an all-zero vector.
    exports.v128_load8_lane(src + 8n, dst);
    check(0n, 0x0000880000000000n);

    exports.v128_load16_lane(src + 8n, dst);
    check(0n, 0x0000998800000000n);

    exports.v128_load32_lane(src + 8n, dst);
    check(0n, 0xBBAA998800000000n);

    exports.v128_load64_lane(src, dst);
    check(0x7766554433221100n, 0n);

    exports.v128_load32_zero(src, dst);
    check(0x33221100n, 0n);

    exports.v128_load64_zero(src, dst);
    check(0x7766554433221100n, 0n);

    exports.v128_store(dst);
    check(0x7766554433221100n, 0xFFEEDDCCBBAA9988n);

    // Lane stores must write only their own lane.
    i64_store(dst, 0n);
    exports.v128_store8_lane(dst);
    assert.eq(i64_load(dst), 0x88n);

    i64_store(dst, 0n);
    exports.v128_store16_lane(dst);
    assert.eq(i64_load(dst), 0x9988n);

    i64_store(dst, 0n);
    exports.v128_store32_lane(dst);
    assert.eq(i64_load(dst), 0xBBAA9988n);

    i64_store(dst, 0n);
    exports.v128_store64_lane(dst);
    assert.eq(i64_load(dst), asI64(0xFFEEDDCCBBAA9988n));
}

function testPageOffset() {
    // Write a vector one page up, then read it back through the same static offset.
    i64_store(pageSize, 0n);
    i64_store(pageSize + 8n, 0n);
    exports.v128_store_page_offset(0n);
    assert.eq(i64_load(pageSize), 0x7766554433221100n);
    assert.eq(i64_load(pageSize + 8n), asI64(0xFFEEDDCCBBAA9988n));

    exports.v128_load_page_offset(0n, 0n);
    assert.eq(i64_load(0n), 0x7766554433221100n);
    assert.eq(i64_load(8n), asI64(0xFFEEDDCCBBAA9988n));
}

// An address whose low 32 bits are in bounds still traps: the high bits must not be
// dropped. 2^32 and 2^32 + 8 alias to 0 and 8 when truncated to 32 bits.
function testAddressAboveFourGigs() {
    for (const address of [hugeOffset, hugeOffset + 8n, 0xFFFFFFFF00000000n, 0x0000000100000010n]) {
        for (const { name } of loadOps)
            outOfBounds(() => exports[name](address, 0n));

        for (const { name } of storeOps)
            outOfBounds(() => exports[name](address));
    }
}

// The same requirement for the static offset, which memory64 encodes as a u64.
function testOffsetAboveFourGigs() {
    for (const { name } of loadOps)
        outOfBounds(() => exports[`${name}_huge_offset`](0n, 0n));

    for (const { name } of storeOps)
        outOfBounds(() => exports[`${name}_huge_offset`](0n));
}

// address + width must be computed without wrapping: 2^64 - 8 plus a 16-byte access
// wraps to 8, which is in bounds.
function testOverflow() {
    for (const { name, width } of loadOps) {
        outOfBounds(() => exports[name](0xFFFFFFFFFFFFFFFFn, 0n));
        outOfBounds(() => exports[name](-width, 0n));
    }

    for (const { name, width } of storeOps) {
        outOfBounds(() => exports[name](0xFFFFFFFFFFFFFFFFn));
        outOfBounds(() => exports[name](-width));
    }
}

// An access ending exactly at the memory bound is in bounds; one byte further is not.
function testBoundary() {
    for (const { name, width } of loadOps) {
        exports[name](memorySize - width, 0n);
        outOfBounds(() => exports[name](memorySize - width + 1n, 0n));
    }

    for (const { name, width } of storeOps) {
        exports[name](memorySize - width);
        outOfBounds(() => exports[name](memorySize - width + 1n));
    }
}

for (let i = 0; i < wasmTestLoopCount; i++) {
    testAt(0n);
    testAt(pageSize);
    testPageOffset();
    testAddressAboveFourGigs();
    testOffsetAboveFourGigs();
    testOverflow();
    testBoundary();
}

// memory.grow takes and returns an i64 page count for memory64; SIMD accesses must
// see the new bound afterwards. Run last: it changes what is in bounds above.
function testGrow() {
    const grownBase = initialPages * pageSize;

    outOfBounds(() => exports.v128_load(grownBase, 0n));
    outOfBounds(() => exports.v128_store(grownBase));

    assert.eq(grow(1n), initialPages);

    i64_store(grownBase, 0x7766554433221100n);
    i64_store(grownBase + 8n, 0xFFEEDDCCBBAA9988n);
    exports.v128_load(grownBase, grownBase + 16n);
    assert.eq(i64_load(grownBase + 16n), 0x7766554433221100n);
    assert.eq(i64_load(grownBase + 24n), asI64(0xFFEEDDCCBBAA9988n));

    const grownSize = maximumPages * pageSize;
    exports.v128_store(grownSize - 16n);
    outOfBounds(() => exports.v128_store(grownSize - 15n));
}

testGrow();
