//@ requireOptions("--useWasmSIMD=1")
//@ skip unless $isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (tag $vtag (export "vtag") (param v128))

    (func (export "testCatchOwnV128") (result i32)
        try (result i32)
            v128.const i32x4 0 0 42 0
            throw $vtag
        catch $vtag
            i32x4.extract_lane 2
        end
    )

    (func $innerFunc
        v128.const i32x4 0 42 0 0
        throw $vtag
    )

    (func (export "testCatchCalleeV128") (result i32)
        try (result i32)
            call $innerFunc
            i32.const 0
        catch $vtag
            i32x4.extract_lane 1
        end
    )

    (tag $interleaved (param v128 i32 v128 f32 v128 i64 v128 f64 v128 f32 i64 v128 f64 i32 v128))

    (func $throwsInterleaved
        v128.const i32x4 1 2 3 4
        i32.const 43
        v128.const i32x4 1 2 3 4
        f32.const 43
        v128.const i32x4 1 2 3 4
        i64.const 43
        v128.const i32x4 1 2 3 4
        f64.const 43
        v128.const i32x4 1 2 3 4
        f32.const 43
        i64.const 43
        v128.const i32x4 1 2 3 4
        f64.const 43
        i32.const 43
        v128.const i32x4 1 2 3 4
        throw $interleaved
    )

    (func (export "testLotsOfInterleavedTypes") (result i32) (local v128 v128 v128 v128 v128 v128 v128)
        try (result i32)
            call $throwsInterleaved
            i32.const 0
        catch $interleaved
            local.set 0
            drop
            drop
            local.set 1
            drop
            drop
            local.set 2
            drop
            local.set 3
            drop
            local.set 4
            drop
            local.set 5
            drop
            local.set 6
            local.get 0
            local.get 1
            local.get 2
            local.get 3
            local.get 4
            local.get 5
            local.get 6
            i32x4.add
            i32x4.add
            i32x4.add
            i32x4.add
            i32x4.add
            i32x4.add
            local.tee 0
            i32x4.extract_lane 1
            local.get 0
            i32x4.extract_lane 3
            i32.add
        end
    )

    (func (export "testThrowV128ToJS")
        v128.const i32x4 0 0 0 0
        throw $vtag
    )
)
`;

async function test() {
    const instance = await instantiate(wat, {}, { exceptions: true, simd: true });
    assert.eq(instance.exports.testCatchOwnV128(), 42);
    assert.eq(instance.exports.testCatchCalleeV128(), 42);
    assert.eq(instance.exports.testLotsOfInterleavedTypes(), 42);
    assert.throws(() => instance.exports.testThrowV128ToJS(), WebAssembly.Exception);
}

await assert.asyncTest(test());
