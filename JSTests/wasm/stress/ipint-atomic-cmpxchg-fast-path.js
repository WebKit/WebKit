//@ skip if $architecture != "arm64" && $architecture != "x86_64"

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

// Exercises the out-of-line non-LSE compare-and-swap helpers: offset < 128 so the fast path in
// loadStoreMakePointerFast is taken, which is where the helper call lives.
let wat = `
(module
  (memory 1 1 shared)
  (func (export "c32")  (param i32 i32 i32) (result i32)
    (local.get 0) (local.get 1) (local.get 2) (i32.atomic.rmw.cmpxchg offset=8))
  (func (export "c64")  (param i32 i64 i64) (result i64)
    (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw.cmpxchg offset=8))
  (func (export "c8u")  (param i32 i32 i32) (result i32)
    (local.get 0) (local.get 1) (local.get 2) (i32.atomic.rmw8.cmpxchg_u offset=8))
  (func (export "c16u") (param i32 i32 i32) (result i32)
    (local.get 0) (local.get 1) (local.get 2) (i32.atomic.rmw16.cmpxchg_u offset=8))
  (func (export "c64_8u")  (param i32 i64 i64) (result i64)
    (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw8.cmpxchg_u offset=8))
  (func (export "c64_16u") (param i32 i64 i64) (result i64)
    (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw16.cmpxchg_u offset=8))
  (func (export "c64_32u") (param i32 i64 i64) (result i64)
    (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw32.cmpxchg_u offset=8))
  (func (export "r32") (param i32) (result i32) (local.get 0) (i32.atomic.load offset=8))
  (func (export "r64") (param i32) (result i64) (local.get 0) (i64.atomic.load offset=8))
  (func (export "w32") (param i32 i32) (local.get 0) (local.get 1) (i32.atomic.store offset=8))
  (func (export "w64") (param i32 i64) (local.get 0) (local.get 1) (i64.atomic.store offset=8))
)`;

async function test() {
    const i = await instantiate(wat, {}, { threads: true });
    const e = i.exports;

    // 32-bit: swap taken, then swap refused. Both must return the prior value.
    e.w32(0, 0x11223344);
    assert.eq(e.c32(0, 0x11223344, 0x55667788), 0x11223344);
    assert.eq(e.r32(0), 0x55667788);
    assert.eq(e.c32(0, 0x00000000, 0x99aabbcc), 0x55667788); // mismatch: no store
    assert.eq(e.r32(0), 0x55667788);

    // 64-bit, including a value with both halves set so a 32-bit-wide loop would be caught.
    e.w64(8, BigInt.asIntN(64, 0x1122334455667788n));
    assert.eq(e.c64(8, BigInt.asIntN(64, 0x1122334455667788n), BigInt.asIntN(64, 0x99aabbccddeeff00n)), BigInt.asIntN(64, 0x1122334455667788n));
    assert.eq(e.r64(8), BigInt.asIntN(64, 0x99aabbccddeeff00n));
    assert.eq(e.c64(8, 0n, 0x1234n), BigInt.asIntN(64, 0x99aabbccddeeff00n));
    assert.eq(e.r64(8), BigInt.asIntN(64, 0x99aabbccddeeff00n));

    // Narrow forms must compare and return only the low bits, leaving neighbours alone.
    e.w32(16, 0x0000abcd);
    assert.eq(e.c16u(16, 0xabcd, 0x1234), 0xabcd);
    assert.eq(e.r32(16), 0x00001234);
    assert.eq(e.c16u(16, 0xffff, 0x5555), 0x1234); // mismatch
    assert.eq(e.r32(16), 0x00001234);

    e.w32(20, 0x000000ef);
    assert.eq(e.c8u(20, 0xef, 0x77), 0xef);
    assert.eq(e.r32(20), 0x00000077);
    assert.eq(e.c8u(20, 0x00, 0x11), 0x77); // mismatch
    assert.eq(e.r32(20), 0x00000077);

    // i64 narrow variants.
    e.w64(24, BigInt.asIntN(64, 0xffffffffffffff42n));
    assert.eq(e.c64_8u(24, 0x42n, 0x43n), 0x42n);
    assert.eq(e.c64_16u(24, 0xff43n, 0xbeefn), 0xff43n);
    assert.eq(e.c64_32u(24, 0xffffbeefn, 0xcafef00dn), 0xffffbeefn);
    assert.eq(e.r64(24), BigInt.asIntN(64, 0xffffffffcafef00dn));
    assert.eq(e.c64_32u(24, 0n, 1n), 0xcafef00dn); // mismatch
    assert.eq(e.r64(24), BigInt.asIntN(64, 0xffffffffcafef00dn));
}
await assert.asyncTest(test());
