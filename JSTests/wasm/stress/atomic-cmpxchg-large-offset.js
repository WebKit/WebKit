//@ skip if $architecture != "arm64" && $architecture != "x86_64"

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

// Test atomic cmpxchg with offset >= 128 to exercise the IPInt slow path.
// When offset >= 128 the LEB128 encoding is multi-byte (first byte >= 0x80),
// causing loadStoreMakePointerFast to fall through to the slow path.
// This specifically tests that expected and replacement are not swapped
// in the slow path's push/pop save/restore sequence.

let wat = `
(module
  (memory 1 1 shared)
  (export "memory" (memory 0))

  (func (export "i32_cmpxchg") (param i32 i32 i32) (result i32)
    (local.get 0) (local.get 1) (local.get 2) (i32.atomic.rmw.cmpxchg offset=128))
  (func (export "i64_cmpxchg") (param i32 i64 i64) (result i64)
    (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw.cmpxchg offset=128))
  (func (export "i32_cmpxchg8") (param i32 i32 i32) (result i32)
    (local.get 0) (local.get 1) (local.get 2) (i32.atomic.rmw8.cmpxchg_u offset=128))
  (func (export "i32_cmpxchg16") (param i32 i32 i32) (result i32)
    (local.get 0) (local.get 1) (local.get 2) (i32.atomic.rmw16.cmpxchg_u offset=128))
  (func (export "i64_cmpxchg8") (param i32 i64 i64) (result i64)
    (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw8.cmpxchg_u offset=128))
  (func (export "i64_cmpxchg16") (param i32 i64 i64) (result i64)
    (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw16.cmpxchg_u offset=128))
  (func (export "i64_cmpxchg32") (param i32 i64 i64) (result i64)
    (local.get 0) (local.get 1) (local.get 2) (i64.atomic.rmw32.cmpxchg_u offset=128))
)
`;

async function test() {
    const instance = await instantiate(wat, {}, { threads: true });
    const e = instance.exports;
    const mem = new DataView(e.memory.buffer);

    // The effective address is param0 + 128. We use param0=0 so effective addr=128.
    const addr = 0;
    const effectiveAddr = 128;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        // i32.atomic.rmw.cmpxchg - success: expected matches memory
        mem.setInt32(effectiveAddr, 10, true);
        assert.eq(e.i32_cmpxchg(addr, 10, 42), 10);    // returns old value
        assert.eq(mem.getInt32(effectiveAddr, true), 42); // memory updated to replacement

        // i32.atomic.rmw.cmpxchg - failure: expected doesn't match memory
        mem.setInt32(effectiveAddr, 10, true);
        assert.eq(e.i32_cmpxchg(addr, 99, 42), 10);     // returns old value (no match)
        assert.eq(mem.getInt32(effectiveAddr, true), 10); // memory unchanged

        // i64.atomic.rmw.cmpxchg - success
        mem.setBigInt64(effectiveAddr, 10n, true);
        assert.eq(e.i64_cmpxchg(addr, 10n, 42n), 10n);
        assert.eq(mem.getBigInt64(effectiveAddr, true), 42n);

        // i64.atomic.rmw.cmpxchg - failure
        mem.setBigInt64(effectiveAddr, 10n, true);
        assert.eq(e.i64_cmpxchg(addr, 99n, 42n), 10n);
        assert.eq(mem.getBigInt64(effectiveAddr, true), 10n);

        // i32.atomic.rmw8.cmpxchg_u - success
        mem.setUint8(effectiveAddr, 10);
        assert.eq(e.i32_cmpxchg8(addr, 10, 42), 10);
        assert.eq(mem.getUint8(effectiveAddr), 42);

        // i32.atomic.rmw8.cmpxchg_u - failure
        mem.setUint8(effectiveAddr, 10);
        assert.eq(e.i32_cmpxchg8(addr, 99, 42), 10);
        assert.eq(mem.getUint8(effectiveAddr), 10);

        // i32.atomic.rmw16.cmpxchg_u - success
        mem.setUint16(effectiveAddr, 10, true);
        assert.eq(e.i32_cmpxchg16(addr, 10, 42), 10);
        assert.eq(mem.getUint16(effectiveAddr, true), 42);

        // i32.atomic.rmw16.cmpxchg_u - failure
        mem.setUint16(effectiveAddr, 10, true);
        assert.eq(e.i32_cmpxchg16(addr, 99, 42), 10);
        assert.eq(mem.getUint16(effectiveAddr, true), 10);

        // i64.atomic.rmw8.cmpxchg_u - success
        mem.setUint8(effectiveAddr, 10);
        assert.eq(e.i64_cmpxchg8(addr, 10n, 42n), 10n);
        assert.eq(mem.getUint8(effectiveAddr), 42);

        // i64.atomic.rmw8.cmpxchg_u - failure
        mem.setUint8(effectiveAddr, 10);
        assert.eq(e.i64_cmpxchg8(addr, 99n, 42n), 10n);
        assert.eq(mem.getUint8(effectiveAddr), 10);

        // i64.atomic.rmw16.cmpxchg_u - success
        mem.setUint16(effectiveAddr, 10, true);
        assert.eq(e.i64_cmpxchg16(addr, 10n, 42n), 10n);
        assert.eq(mem.getUint16(effectiveAddr, true), 42);

        // i64.atomic.rmw16.cmpxchg_u - failure
        mem.setUint16(effectiveAddr, 10, true);
        assert.eq(e.i64_cmpxchg16(addr, 99n, 42n), 10n);
        assert.eq(mem.getUint16(effectiveAddr, true), 10);

        // i64.atomic.rmw32.cmpxchg_u - success
        mem.setUint32(effectiveAddr, 10, true);
        assert.eq(e.i64_cmpxchg32(addr, 10n, 42n), 10n);
        assert.eq(mem.getUint32(effectiveAddr, true), 42);

        // i64.atomic.rmw32.cmpxchg_u - failure
        mem.setUint32(effectiveAddr, 10, true);
        assert.eq(e.i64_cmpxchg32(addr, 99n, 42n), 10n);
        assert.eq(mem.getUint32(effectiveAddr, true), 10);
    }
}

await assert.asyncTest(test());
