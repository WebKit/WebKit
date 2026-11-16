//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform

// A memory32 access folds a 32-bit index and an unsigned 32-bit immediate offset in 64-bit
// arithmetic, so it can reach far above 4GiB. Signaling memories reserve 4GiB plus a redzone
// and omit the explicit bounds check for offsets the redzone can absorb, which is only sound
// while the whole access stays inside the reservation. Offsets here bracket the redzone size
// so that widening the set of accesses that skip the check turns into a missing trap rather
// than a read or write reaching whatever the next reservation holds.

import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const pageSize = 65536;
const iterations = 5;

const accesses = [
    { size: 1, load: "i32.load8_u", store: "i32.store8", type: "i32", stored: "(i32.const 0xa5)", expected: 0xa5 },
    { size: 2, load: "i32.load16_u", store: "i32.store16", type: "i32", stored: "(i32.const 0xa5a5)", expected: 0xa5a5 },
    { size: 4, load: "i32.load", store: "i32.store", type: "i32", stored: "(i32.const 0x12345678)", expected: 0x12345678 },
    { size: 8, load: "i64.load", store: "i64.store", type: "i64", stored: "(i64.const 0x123456789abcdef0)", expected: 0x123456789abcdef0n },
    { size: 16, load: "v128.load", store: "v128.store", type: "i64", stored: "(v128.const i64x2 0x123456789abcdef0 0x123456789abcdef0)", expected: 0x123456789abcdef0n },
];

// The redzone defaults to 128 pages, putting its end at 0x800000. The expectations below do
// not depend on that: an access is out of bounds exactly when its last byte leaves the memory.
const offsets = [0, 1, 0xffff, 0x10000, 0x7ffff1, 0x7ffff8, 0x7fffff, 0x800000, 0x800001, 0x1000000, 0x7fffffff, 0xffffffff];
const indices = [0, 1, 0xffff, 0x10000, 0x7fffffff, 0xffffffff];

function loadExpression(access, offset) {
    const load = `(${access.load} offset=${offset} (local.get 0))`;
    return access.size === 16 ? `(i64x2.extract_lane 0 ${load})` : load;
}

function instantiateForPages(pages) {
    let functions = "";
    for (const access of accesses) {
        for (const offset of offsets) {
            functions += `
                (func (export "load${access.size}_${offset}") (param i32) (result ${access.type})
                    ${loadExpression(access, offset)})
                (func (export "store${access.size}_${offset}") (param i32)
                    (${access.store} offset=${offset} (local.get 0) ${access.stored}))`;
        }
    }
    return instantiate(`(module (memory ${pages}) ${functions})`, {});
}

async function testPages(pages) {
    const instance = await instantiateForPages(pages);
    const limit = pages * pageSize;

    for (const access of accesses) {
        for (const offset of offsets) {
            const load = instance.exports[`load${access.size}_${offset}`];
            const store = instance.exports[`store${access.size}_${offset}`];
            for (const index of indices) {
                const argument = index | 0;
                if (index + offset + access.size - 1 >= limit) {
                    for (let i = 0; i < iterations; ++i) {
                        assert.throws(() => load(argument), WebAssembly.RuntimeError, "Out of bounds memory access");
                        assert.throws(() => store(argument), WebAssembly.RuntimeError, "Out of bounds memory access");
                    }
                } else {
                    for (let i = 0; i < iterations; ++i) {
                        store(argument);
                        assert.eq(load(argument), access.expected);
                    }
                }
            }
        }
    }
}

async function test() {
    // One page leaves every large offset out of bounds; 300 pages reaches past the default
    // redzone, so the offsets that need an explicit check are also exercised in bounds.
    await testPages(1);
    await testPages(300);
}

await assert.asyncTest(test());
