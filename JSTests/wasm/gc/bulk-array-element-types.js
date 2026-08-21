import * as assert from "../assert.js";
import { instantiate } from "./wast-wrapper.js";

// array.copy and array.fill are lowered per element type, and the length is only known to be
// non-zero on one side of a branch, so exercise every element type against a constant length, a
// runtime length and a zero length.

// A packed element is written and read as an i32, so its operand type differs from its storage type.
const cases = [
    { type: "i8", valueType: "i32", get: "array.get_u", zero: 0, fill: 0x41 },
    { type: "i16", valueType: "i32", get: "array.get_u", zero: 0, fill: 0x4142 },
    { type: "i32", valueType: "i32", get: "array.get", zero: 0, fill: 0x41424344 },
    { type: "i64", valueType: "i64", get: "array.get", zero: 0n, fill: 0x4142434445464748n },
    { type: "f32", valueType: "f32", get: "array.get", zero: 0, fill: 1.5 },
    { type: "f64", valueType: "f64", get: "array.get", zero: 0, fill: 1.5 },
];

function module({ type, valueType, get }, length) {
    return instantiate(`
        (module
           (type $arr (array (mut ${type})))
           (global $a (mut (ref null $arr)) (ref.null $arr))
           (global $b (mut (ref null $arr)) (ref.null $arr))
           (func (export "reset")
             (global.set $a (array.new_default $arr (i32.const 16)))
             (global.set $b (array.new_default $arr (i32.const 16))))
           (func (export "fillConst") (param ${valueType})
             (array.fill $arr (global.get $a) (i32.const 2) (local.get 0) (i32.const ${length})))
           (func (export "fill") (param ${valueType}) (param i32)
             (array.fill $arr (global.get $a) (i32.const 2) (local.get 0) (local.get 1)))
           (func (export "copyConst")
             (array.copy $arr $arr (global.get $b) (i32.const 3) (global.get $a) (i32.const 2) (i32.const ${length})))
           (func (export "copy") (param i32)
             (array.copy $arr $arr (global.get $b) (i32.const 3) (global.get $a) (i32.const 2) (local.get 0)))
           (func (export "copyWithin") (param i32 i32 i32)
             (array.copy $arr $arr (global.get $a) (local.get 0) (global.get $a) (local.get 1) (local.get 2)))
           (func (export "getA") (param i32) (result ${valueType}) (${get} $arr (global.get $a) (local.get 0)))
           (func (export "getB") (param i32) (result ${valueType}) (${get} $arr (global.get $b) (local.get 0)))
           (func (export "setA") (param i32 ${valueType}) (array.set $arr (global.get $a) (local.get 0) (local.get 1))))
    `).exports;
}

for (const c of cases) {
    const length = 5;
    // reset() installs fresh default-initialized arrays, so one module covers every scenario without
    // depending on the instructions under test to clear them.
    const m = module(c, length);

    // A constant length lets the fill and the copy be emitted without a call at all.
    m.reset();
    m.fillConst(c.fill);
    for (let i = 0; i < 16; ++i)
        assert.eq(m.getA(i), i >= 2 && i < 2 + length ? c.fill : c.zero);

    m.copyConst();
    for (let i = 0; i < 16; ++i)
        assert.eq(m.getB(i), i >= 3 && i < 3 + length ? c.fill : c.zero);

    // The same work with the length only known at runtime.
    m.reset();
    m.fill(c.fill, length);
    for (let i = 0; i < 16; ++i)
        assert.eq(m.getA(i), i >= 2 && i < 2 + length ? c.fill : c.zero);

    m.copy(length);
    for (let i = 0; i < 16; ++i)
        assert.eq(m.getB(i), i >= 3 && i < 3 + length ? c.fill : c.zero);

    // A zero length is in bounds as long as neither offset is past the end, and must not write.
    m.reset();
    m.fill(c.fill, 0);
    m.copy(0);
    for (let i = 0; i < 16; ++i) {
        assert.eq(m.getA(i), c.zero);
        assert.eq(m.getB(i), c.zero);
    }

    // Overlapping ranges within one array move in both directions. A distinct value per element is
    // what makes a copy that walks in the wrong direction observable.
    for (const [dstOffset, srcOffset] of [[0, 4], [4, 0]]) {
        m.reset();
        const before = [];
        for (let i = 0; i < 16; ++i) {
            m.setA(i, c.valueType === "i64" ? BigInt(i + 1) : i + 1);
            before.push(m.getA(i));
        }

        m.copyWithin(dstOffset, srcOffset, 8);
        for (let i = 0; i < 16; ++i) {
            const expected = i >= dstOffset && i < dstOffset + 8 ? before[srcOffset + (i - dstOffset)] : before[i];
            assert.eq(m.getA(i), expected);
        }
    }
}

// Reference elements go through a copy that stores whole references, and need a write barrier.
{
    const m = instantiate(`
        (module
           (type $arr (array (mut anyref)))
           (type $box (struct (field i32)))
           (global $a (ref $arr) (array.new_default $arr (i32.const 8)))
           (global $b (ref $arr) (array.new_default $arr (i32.const 8)))
           (func (export "fill") (param i32) (param i32)
             (array.fill $arr (global.get $a) (i32.const 1) (struct.new $box (local.get 0)) (local.get 1)))
           (func (export "copy") (param i32)
             (array.copy $arr $arr (global.get $b) (i32.const 2) (global.get $a) (i32.const 1) (local.get 0)))
           (func (export "copyWithin") (param i32 i32 i32)
             (array.copy $arr $arr (global.get $a) (local.get 0) (global.get $a) (local.get 1) (local.get 2)))
           (func (export "setA") (param i32 i32)
             (array.set $arr (global.get $a) (local.get 0) (struct.new $box (local.get 1))))
           (func (export "getA") (param i32) (result i32)
             (struct.get $box 0 (ref.cast (ref $box) (array.get $arr (global.get $a) (local.get 0)))))
           (func (export "getB") (param i32) (result i32)
             (struct.get $box 0 (ref.cast (ref $box) (array.get $arr (global.get $b) (local.get 0)))))
           (func (export "isNullA") (param i32) (result i32)
             (ref.is_null (array.get $arr (global.get $a) (local.get 0))))
           (func (export "isNullB") (param i32) (result i32)
             (ref.is_null (array.get $arr (global.get $b) (local.get 0)))))
    `).exports;

    m.fill(7, 4);
    for (let i = 0; i < 8; ++i) {
        if (i >= 1 && i < 5) {
            assert.eq(m.isNullA(i), 0);
            assert.eq(m.getA(i), 7);
        } else
            assert.eq(m.isNullA(i), 1);
    }

    m.copy(4);
    for (let i = 0; i < 8; ++i) {
        if (i >= 2 && i < 6) {
            assert.eq(m.isNullB(i), 0);
            assert.eq(m.getB(i), 7);
        } else
            assert.eq(m.isNullB(i), 1);
    }

    // A zero-length reference copy must not disturb the destination.
    m.copy(0);
    for (let i = 0; i < 8; ++i)
        assert.eq(m.isNullB(i), i >= 2 && i < 6 ? 0 : 1);

    // References move through a GC-safe memmove, so overlapping ranges have to walk in the direction
    // that preserves the source.
    for (const [dstOffset, srcOffset] of [[0, 3], [3, 0]]) {
        const before = [];
        for (let i = 0; i < 8; ++i) {
            m.setA(i, i + 1);
            before.push(i + 1);
        }

        m.copyWithin(dstOffset, srcOffset, 5);
        for (let i = 0; i < 8; ++i) {
            const expected = i >= dstOffset && i < dstOffset + 5 ? before[srcOffset + (i - dstOffset)] : before[i];
            assert.eq(m.getA(i), expected);
        }
    }
}

// A v128 fill passes its two lanes separately, and a v128 payload is found by rounding the address
// up at runtime rather than by a fixed offset.
{
    const m = instantiate(`
        (module
           (type $arr (array (mut v128)))
           (global $a (ref $arr) (array.new_default $arr (i32.const 8)))
           (global $b (ref $arr) (array.new_default $arr (i32.const 8)))
           (func (export "fill") (param i32)
             (array.fill $arr (global.get $a) (i32.const 1)
               (v128.const i64x2 0x0102030405060708 0x1112131415161718)
               (local.get 0)))
           (func (export "copy") (param i32)
             (array.copy $arr $arr (global.get $b) (i32.const 2) (global.get $a) (i32.const 1) (local.get 0)))
           (func (export "getA0") (param i32) (result i64)
             (i64x2.extract_lane 0 (array.get $arr (global.get $a) (local.get 0))))
           (func (export "getA1") (param i32) (result i64)
             (i64x2.extract_lane 1 (array.get $arr (global.get $a) (local.get 0))))
           (func (export "getB0") (param i32) (result i64)
             (i64x2.extract_lane 0 (array.get $arr (global.get $b) (local.get 0))))
           (func (export "getB1") (param i32) (result i64)
             (i64x2.extract_lane 1 (array.get $arr (global.get $b) (local.get 0)))))
    `).exports;

    const lane0 = 0x0102030405060708n;
    const lane1 = 0x1112131415161718n;

    m.fill(4);
    for (let i = 0; i < 8; ++i) {
        const filled = i >= 1 && i < 5;
        assert.eq(m.getA0(i), filled ? lane0 : 0n);
        assert.eq(m.getA1(i), filled ? lane1 : 0n);
    }

    m.copy(4);
    for (let i = 0; i < 8; ++i) {
        const copied = i >= 2 && i < 6;
        assert.eq(m.getB0(i), copied ? lane0 : 0n);
        assert.eq(m.getB1(i), copied ? lane1 : 0n);
    }

    // A zero length must leave both arrays alone.
    m.fill(0);
    m.copy(0);
    for (let i = 0; i < 8; ++i) {
        assert.eq(m.getA0(i), i >= 1 && i < 5 ? lane0 : 0n);
        assert.eq(m.getB0(i), i >= 2 && i < 6 ? lane0 : 0n);
    }
}

// offset + size is checked without wrapping, so a size that would overflow a 32-bit sum still traps
// rather than being treated as in bounds.
{
    const m = instantiate(`
        (module
           (type $arr (array (mut i32)))
           (global $a (ref $arr) (array.new_default $arr (i32.const 8)))
           (func (export "fill") (param i32 i32)
             (array.fill $arr (global.get $a) (local.get 0) (i32.const 1) (local.get 1)))
           (func (export "copy") (param i32 i32 i32)
             (array.copy $arr $arr (global.get $a) (local.get 0) (global.get $a) (local.get 1) (local.get 2))))
    `).exports;

    for (const [offset, size] of [[1, -1], [-1, 1], [-1, -1], [0, 9], [8, 1], [9, 0]]) {
        assert.throws(() => m.fill(offset, size), WebAssembly.RuntimeError, "Out of bounds array.fill");
        assert.throws(() => m.copy(offset, 0, size), WebAssembly.RuntimeError, "Out of bounds array.copy");
        assert.throws(() => m.copy(0, offset, size), WebAssembly.RuntimeError, "Out of bounds array.copy");
    }

    // A zero-length range at the very end of the array is in bounds.
    m.fill(8, 0);
    m.copy(8, 8, 0);
}

// A constant offset and a constant size are extended to 64 bits by hand, so repeat the range check
// with every operand constant.
for (const [offset, size] of [[1, -1], [-1, 1], [-1, -1], [0, 9], [8, 1], [9, 0]]) {
    const m = instantiate(`
        (module
           (type $arr (array (mut i32)))
           (global $a (ref $arr) (array.new_default $arr (i32.const 8)))
           (func (export "fill")
             (array.fill $arr (global.get $a) (i32.const ${offset}) (i32.const 1) (i32.const ${size})))
           (func (export "copy")
             (array.copy $arr $arr (global.get $a) (i32.const ${offset}) (global.get $a) (i32.const 0) (i32.const ${size}))))
    `).exports;

    assert.throws(() => m.fill(), WebAssembly.RuntimeError, "Out of bounds array.fill");
    assert.throws(() => m.copy(), WebAssembly.RuntimeError, "Out of bounds array.copy");
}

// A constant fill value whose bytes all repeat is lowered to a byte-wise fill, which needs the
// element count scaled to a byte count. A float constant reaches the same path through its bits.
{
    const m = instantiate(`
        (module
           (type $i32arr (array (mut i32)))
           (type $i64arr (array (mut i64)))
           (type $f64arr (array (mut f64)))
           (global $i32 (ref $i32arr) (array.new_default $i32arr (i32.const 8)))
           (global $i64 (ref $i64arr) (array.new_default $i64arr (i32.const 8)))
           (global $f64 (ref $f64arr) (array.new_default $f64arr (i32.const 8)))
           (func (export "fillI32")
             (array.fill $i32arr (global.get $i32) (i32.const 1) (i32.const 0x41414141) (i32.const 5)))
           (func (export "fillI64")
             (array.fill $i64arr (global.get $i64) (i32.const 1) (i64.const -1) (i32.const 5)))
           (func (export "setF64") (param i32 f64)
             (array.set $f64arr (global.get $f64) (local.get 0) (local.get 1)))
           (func (export "fillF64")
             (array.fill $f64arr (global.get $f64) (i32.const 1) (f64.const 0) (i32.const 5)))
           (func (export "getI32") (param i32) (result i32) (array.get $i32arr (global.get $i32) (local.get 0)))
           (func (export "getI64") (param i32) (result i64) (array.get $i64arr (global.get $i64) (local.get 0)))
           (func (export "getF64") (param i32) (result f64) (array.get $f64arr (global.get $f64) (local.get 0))))
    `).exports;

    for (let i = 0; i < 8; ++i)
        m.setF64(i, 1.5);

    m.fillI32();
    m.fillI64();
    m.fillF64();

    for (let i = 0; i < 8; ++i) {
        const filled = i >= 1 && i < 6;
        assert.eq(m.getI32(i), filled ? 0x41414141 : 0);
        assert.eq(m.getI64(i), filled ? -1n : 0n);
        assert.eq(m.getF64(i), filled ? 0 : 1.5);
    }
}

// A null array traps on the length load that the bounds check needs, so it reports a plain null
// access rather than a message naming the instruction.
{
    const m = instantiate(`
        (module
           (type $arr (array (mut i32)))
           (global $null (ref null $arr) (ref.null $arr))
           (func (export "fill") (param i32)
             (array.fill $arr (global.get $null) (i32.const 0) (i32.const 1) (local.get 0)))
           (func (export "copyDst") (param i32)
             (array.copy $arr $arr (global.get $null) (i32.const 0) (array.new_default $arr (i32.const 4)) (i32.const 0) (local.get 0)))
           (func (export "copySrc") (param i32)
             (array.copy $arr $arr (array.new_default $arr (i32.const 4)) (i32.const 0) (global.get $null) (i32.const 0) (local.get 0))))
    `).exports;

    for (const size of [0, 1]) {
        assert.throws(() => m.fill(size), WebAssembly.RuntimeError, "access to a null reference");
        assert.throws(() => m.copyDst(size), WebAssembly.RuntimeError, "access to a null reference");
        assert.throws(() => m.copySrc(size), WebAssembly.RuntimeError, "access to a null reference");
    }
}
