import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// LICM may hoist a trapping load out of a loop, but only to a point where the load was going to
// run anyway and where nothing observable happens first. These check the cases where hoisting
// would be wrong.

// The load is guarded by a condition that is never true, so it must never run. Hoisting it to the
// pre-header unconditionally would turn a clean return into a trap.
let watNeverTaken = `
(module
    (memory 1)
    (func (export "test") (param $iterations i32) (param $addr i32) (result i32)
        (local $i i32)
        (loop $loop
            local.get $i
            i32.const -1
            i32.eq
            if
                local.get $addr
                i32.load
                drop
            end
            local.get $i
            i32.const 1
            i32.add
            local.set $i
            local.get $i
            local.get $iterations
            i32.lt_s
            br_if $loop
        )
        i32.const 42
    )
)
`;

// A wasm trap tears down to the entry frame, so stores the loop already made stay visible to the
// embedder. The load may only trap after the iteration that stored, never before.
let watStoresStayVisible = `
(module
    (memory (export "mem") 1)
    (func (export "test") (param $trapAt i32) (param $addr i32)
        (local $i i32)
        (loop $loop
            i32.const 0
            i32.const 0
            i32.load
            i32.const 1
            i32.add
            i32.store

            local.get $i
            local.get $trapAt
            i32.eq
            if
                local.get $addr
                i32.load
                drop
            end

            local.get $i
            i32.const 1
            i32.add
            local.set $i
            local.get $i
            local.get $trapAt
            i32.le_s
            br_if $loop
        )
    )
)
`;

// A loop-invariant load sharing a try with a call that throws every iteration. The catch edge is a
// side exit inside the loop, which is the case the pass used to reject outright.
let watLoadInTry = `
(module
    (memory 1)
    (tag $e)
    (func $thrower (throw $e))
    (func (export "test") (param $iterations i32) (param $addr i32) (result i32)
        (local $i i32)
        (local $caught i32)
        (loop $loop
            try
                local.get $addr
                i32.load
                drop
                call $thrower
            catch_all
                local.get $caught
                i32.const 1
                i32.add
                local.set $caught
            end
            local.get $i
            i32.const 1
            i32.add
            local.set $i
            local.get $i
            local.get $iterations
            i32.lt_s
            br_if $loop
        )
        local.get $caught
    )
)
`;

const iterations = 2000;
const unmappedAddress = 0xFFFFFF0;

async function test() {
    {
        const { test } = (await instantiate(watNeverTaken, {}, {})).exports;
        for (let i = 0; i < 10; ++i)
            assert.eq(test(iterations, unmappedAddress), 42);
    }

    {
        const instance = await instantiate(watStoresStayVisible, {}, {});
        const { test, mem } = instance.exports;
        const view = new Int32Array(mem.buffer);
        const trapAt = 500;
        assert.throws(() => test(trapAt, unmappedAddress), WebAssembly.RuntimeError, "Out of bounds memory access");
        assert.eq(view[0], trapAt + 1);
    }

    {
        const { test } = (await instantiate(watLoadInTry, {}, { exceptions: true })).exports;
        for (let i = 0; i < 10; ++i)
            assert.eq(test(iterations, 0), iterations);
    }
}

await assert.asyncTest(test());
