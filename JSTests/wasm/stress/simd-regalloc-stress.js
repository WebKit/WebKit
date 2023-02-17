//@ requireOptions("--useWebAssemblySIMD=1", "--thresholdForOMGOptimizeSoon=1", "--thresholdForOMGOptimizeAfterWarmUp=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

/*
This test stresses the UD register on Arm64 for bitselect.

Air     MoveVector (%tmp0), %ftmp12
Air     MoveVector 16(%tmp0), %ftmp13
Air     MoveVector 32(%tmp0), %ftmp11
Air     MoveVector %ftmp11, %ftmp10
Air     VectorBitwiseSelect %ftmp12, %ftmp13, %ftmp10
Air     MoveVector %ftmp10, %*ftmp8*
(ftmp11, ftmp10 are unspillable here because it is Use'd right after being Def'd, and the range is size 1)
...
Air     Patch &CCall1, %tmp1, %x0, %x1, %q0, %x0, %x1
Air     MoveDoubleConditionallyTest32 NonZero, %tmp4, %tmp4, %*ftmp8*, %ftmp9, %ftmp6

Then, after this iteration, we coalesce ftmp11 -> ftmp10 -> ftmp8:

Air     MoveVector 32(%x23), %ftmp11
Air     VectorBitwiseSelect %ftmp12, %ftmp13, %ftmp11

But ftmp11 is still unspillable, and now the graph is uncolourable.

Instead, we should emit this:

Air     MoveVector %ftmp8, %ftmp6
Air     VectorBitwiseSelect %ftmp9, %ftmp10, %ftmp6
...
Air     MoveDoubleConditionallyTest32 NonZero, %x21, %x21, %ftmp6, %ftmp7, %ftmp4

Here, the register allocator can see the lifetime of ftmp6 right away, so we always consider spilling it.
*/

let wat = `
(module
    (memory 1566 2420)
    (func $test_v (export "test_v") (result v128)
        (v128.bitselect
            (v128.const i32x4 0xc5390eea 0x380f028d 0x0783e59c 0x42450ed6)
            (v128.const i32x4 0x06af5fea 0x88b2a9c9 0x464d61fd 0xa45d8e65)
            (v128.const i32x4 0xaf7bd660 0xadc482ad 0x950b56f5 0x6eca2081))
    
        (v128.const i32x4 0xc4b8b1c4 0x61f689d9 0xd0aa3e6b 0x9664e1e7)
        (memory.size)
        (ref.func $test_v)
        (drop)
        (select)
    
        (v128.const i32x4 0xa87ec07f 0x862e0e2c 0xe8540566 0x681d8726)    
        (v128.const i32x4 0x2f0f7b81 0xdf627e48 0x9df8369b 0xe6126ff6)
        (v128.bitselect)
        (v128.const i32x4 0xd5dfc95a 0x718c317c 0xec507a32 0x8acdd83f)
        (ref.func $test_v)
        (drop)
        (v128.const i32x4 0x2f0f7b81 0xdf627e48 0x9df8369b 0xe6126ff6)
        (v128.bitselect)
    )

    (func $test (export "test")
        (call $test_v)
        (drop)
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test, test2 } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(test(), undefined)
    }
}

assert.asyncTest(test())
