//@ runDefaultWasm("-m", "--useConcurrentJIT=0")

// A byte-at-a-time copy or fill loop can be entered by OSR part way through its run, with the
// pointers and the counter already advanced. What is left of the run then has to come out the same
// whether it is finished by the loop or by the bulk memory operation that replaces it, so each
// function here runs a count long enough to tier up in the middle of the loop.

import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const PAGE = 65536;
const COUNT = 30000;

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

async function test()
{
    const instance = await instantiate(wat, {}, {});
    const { copyForward, copyBackward, fill, readD, readS, readN, mem } = instance.exports;
    const memory = new Uint8Array(mem.buffer);
    const expected = new Uint8Array(PAGE);

    function seed()
    {
        memory.set(golden);
        expected.set(golden);
    }

    function assertMemoryMatches()
    {
        let i = 0;
        while (i < PAGE && memory[i] === expected[i])
            ++i;
        assert.eq(i, PAGE, "memory differs at index " + i);
    }

    // Overlapping with the destination below the source, which a bulk copy performs faithfully.
    function copyMemmoveLike()
    {
        seed();
        copyForward(0, 20000, COUNT);
        expected.copyWithin(0, 20000, 20000 + COUNT);
        assert.eq([readD(), readS(), readN()], [COUNT, 20000 + COUNT, 0]);
        assertMemoryMatches();
    }

    // Overlapping the other way, which replicates a byte across the destination rather than moving
    // the region, so the loop has to keep running after the OSR entry.
    function copyReplicating()
    {
        seed();
        copyForward(20000, 19999, COUNT);
        for (let i = 0; i < COUNT; ++i)
            expected[20000 + i] = expected[19999 + i];
        assert.eq([readD(), readS(), readN()], [20000 + COUNT, 19999 + COUNT, 0]);
        assertMemoryMatches();
    }

    function copyDownwards()
    {
        seed();
        copyBackward(PAGE, PAGE - COUNT, COUNT);
        expected.copyWithin(PAGE - COUNT, PAGE - 2 * COUNT, PAGE - COUNT);
        assert.eq([readD(), readS(), readN()], [PAGE - COUNT, PAGE - 2 * COUNT, 0]);
        assertMemoryMatches();
    }

    function fillRegion()
    {
        seed();
        fill(1000, 0x5a, COUNT);
        expected.fill(0x5a, 1000, 1000 + COUNT);
        assert.eq([readD(), readN()], [1000 + COUNT, 0]);
        assertMemoryMatches();
    }

    // The OSR entry happens on the run where the loop's own back edges tier the function up, which
    // takes more than one run of a loop this long.
    for (let repeat = 0; repeat < 4; ++repeat) {
        copyMemmoveLike();
        copyReplicating();
        copyDownwards();
        fillRegion();
    }
}

await assert.asyncTest(test());
