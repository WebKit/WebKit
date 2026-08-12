// Exercises the recognition of loops that copy or fill linear memory one byte per iteration,
// which OMG compiles as a bulk memory operation instead. The bulk form is only faithful to the
// loop for some inputs, so every case here is checked against a JavaScript model of the loop
// itself: copies that overlap the wrong way replicate bytes rather than move them, a copy that
// runs off the end of memory traps, and the loop leaves its pointer and counter locals at
// specific values.

import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const PAGE = 65536;

// Every case in the matrix below touches memory inside this window, which leaves room on both sides
// for a bulk operation that wrote outside the loop's range to show up as a difference.
const WINDOW_BEGIN = 512;
const WINDOW_END = 3072;

const golden = new Uint8Array(PAGE);
for (let i = 0; i < PAGE; ++i)
    golden[i] = (i * 31 + 7) & 0xff;

let wat = `
(module
    (memory (export "mem") 1)
    (global $d (mut i32) (i32.const 0))
    (global $s (mut i32) (i32.const 0))
    (global $n (mut i32) (i32.const 0))

    ;; for (; n; --n) mem[d++] = mem[s++];
    (func (export "copyForward") (param i32 i32 i32)
        (local i32)
        local.get 2
        if
            local.get 0
            local.set 3
            loop
                local.get 3
                local.get 1
                i32.load8_u
                i32.store8
                local.get 3
                i32.const 1
                i32.add
                local.set 3
                local.get 1
                i32.const 1
                i32.add
                local.set 1
                local.get 2
                i32.const 1
                i32.sub
                local.tee 2
                br_if 0
            end
        end
        local.get 3
        global.set $d
        local.get 1
        global.set $s
        local.get 2
        global.set $n)

    ;; for (; n; --n) mem[--d] = mem[--s];  -- the caller passes the ends of the regions.
    (func (export "copyBackward") (param i32 i32 i32)
        (local i32)
        local.get 2
        if
            local.get 0
            local.set 3
            loop
                local.get 3
                i32.const 1
                i32.sub
                local.tee 3
                local.get 1
                i32.const 1
                i32.sub
                local.tee 1
                i32.load8_u
                i32.store8
                local.get 2
                i32.const 1
                i32.sub
                local.tee 2
                br_if 0
            end
        end
        local.get 3
        global.set $d
        local.get 1
        global.set $s
        local.get 2
        global.set $n)

    ;; for (; n; --n) mem[d++] = v;
    (func (export "fill") (param i32 i32 i32)
        (local i32)
        local.get 2
        if
            local.get 0
            local.set 3
            loop
                local.get 3
                local.get 1
                i32.store8
                local.get 3
                i32.const 1
                i32.add
                local.set 3
                local.get 2
                i32.const 1
                i32.sub
                local.tee 2
                br_if 0
            end
        end
        local.get 3
        global.set $d
        local.get 2
        global.set $n)

    (func (export "readD") (result i32) global.get $d)
    (func (export "readS") (result i32) global.get $s)
    (func (export "readN") (result i32) global.get $n)
)
`;

// The models throw where the wasm loop would trap, so the same function serves both the in-bounds
// cases and the out-of-bounds ones.
function access(i)
{
    if (i < 0 || i >= PAGE)
        throw new RangeError("out of bounds");
    return i;
}

function modelCopyForward(memory, d, s, n)
{
    if (n) {
        do {
            memory[access(d)] = memory[access(s)];
            d = (d + 1) | 0;
            s = (s + 1) | 0;
        } while (--n);
    }
    return [d, s, n];
}

function modelCopyBackward(memory, d, s, n)
{
    if (n) {
        do {
            d = (d - 1) | 0;
            s = (s - 1) | 0;
            memory[access(d)] = memory[access(s)];
        } while (--n);
    }
    return [d, s, n];
}

function modelFill(memory, d, v, n)
{
    if (n) {
        do {
            memory[access(d)] = v & 0xff;
            d = (d + 1) | 0;
        } while (--n);
    }
    return [d, n];
}

// Lengths bracket the sizes at which a bulk copy switches between vector, 64-bit, 32-bit and
// byte moves, and the addresses cover overlap in both directions at several distances.
const lengths = [1, 2, 3, 7, 8, 15, 16, 17, 31, 63, 127, 128, 129, 300];
const addressPairs = [[1000, 2000], [2000, 1000], [1000, 1001], [1001, 1000], [1000, 1003], [1003, 1000], [1000, 1000]];

async function test()
{
    const instance = await instantiate(wat, {}, {});
    const { copyForward, copyBackward, fill, readD, readS, readN, mem } = instance.exports;
    const memory = new Uint8Array(mem.buffer);
    const expected = new Uint8Array(PAGE);

    const goldenWindow = golden.subarray(WINDOW_BEGIN, WINDOW_END);

    function seedWindow()
    {
        memory.set(goldenWindow, WINDOW_BEGIN);
        expected.set(goldenWindow, WINDOW_BEGIN);
    }

    function seedEverything()
    {
        memory.set(golden);
        expected.set(golden);
    }

    function assertMemoryMatches(begin, end)
    {
        let i = begin;
        while (i < end && memory[i] === expected[i])
            ++i;
        assert.eq(i, end, "memory differs at index " + i);
    }

    function checkEveryCase()
    {
        for (const n of lengths) {
            for (const [d, s] of addressPairs.concat([[1000, 1000 + n], [1000 + n, 1000]])) {
                seedWindow();
                copyForward(d, s, n);
                assert.eq(modelCopyForward(expected, d, s, n), [readD(), readS(), readN()]);
                assertMemoryMatches(WINDOW_BEGIN, WINDOW_END);

                seedWindow();
                copyBackward(d, s, n);
                assert.eq(modelCopyBackward(expected, d, s, n), [readD(), readS(), readN()]);
                assertMemoryMatches(WINDOW_BEGIN, WINDOW_END);

                seedWindow();
                fill(d, s, n);
                assert.eq(modelFill(expected, d, s, n), [readD(), readN()]);
                assertMemoryMatches(WINDOW_BEGIN, WINDOW_END);
            }
        }
    }

    // Check every case below OMG, then warm the functions up until OMG compiles them and check every
    // case again there, since only OMG replaces the loops with a bulk operation.
    for (let repeat = 0; repeat < 2; ++repeat) {
        checkEveryCase();
        for (let i = 0; i < wasmTestLoopCount; ++i) {
            copyForward(1000, 2000, 64);
            copyBackward(1500, 2500, 64);
            fill(1000, 0x5a, 64);
        }
    }
    checkEveryCase();

    // A copy that runs off the end of memory must trap. How far it got before trapping is left
    // unchecked, since the bulk operation and the loop need not agree on that.
    function assertTraps(copy, model, d, s, n)
    {
        seedEverything();
        assert.throws(() => model(expected, d, s, n), RangeError, "out of bounds");
        assert.throws(() => copy(d, s, n), WebAssembly.RuntimeError, "Out of bounds memory access");
    }
    assertTraps(copyForward, modelCopyForward, PAGE - 10, 0, 40);
    assertTraps(copyForward, modelCopyForward, 0, PAGE - 10, 40);
    assertTraps(copyBackward, modelCopyBackward, 10, 4000, 40);
    assertTraps(copyBackward, modelCopyBackward, 4000, 10, 40);

    // Right up against the end of memory is in bounds and must not trap.
    seedEverything();
    copyForward(PAGE - 40, 0, 40);
    modelCopyForward(expected, PAGE - 40, 0, 40);
    assertMemoryMatches(0, PAGE);
}

await assert.asyncTest(test());
