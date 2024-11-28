//@ requireOptions("--useWasmSIMD=1")
//@ skip unless $isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (import "tags" "vtag" (tag $vtag (param v128)))
    (import "tags" "vvtag" (tag $vvtag (param v128 v128)))
    (import "tags" "vitag" (tag $vitag (param v128 i32)))

    (func (export "testThrowV128") (result i32)
        try (result i32)
            v128.const i32x4 0 0 42 0
            throw $vtag
        catch $vtag
            i32x4.extract_lane 2
        end
    )

    (func (export "testThrowV128Pair") (result i32)
        try (result i32)
            v128.const i32x4 0 21 42 0
            v128.const i32x4 0 21 42 0
            throw $vvtag
        catch $vvtag
            i32x4.add
            i32x4.extract_lane 1
        end
    )

    (func (export "testThrowV128AndI32") (result i32)
        try (result i32)
            v128.const i32x4 1 2 3 4
            i32.const 42
            throw $vitag
        catch $vitag
            br 0
        end
    )
)
`;

async function test() {
    const vtag = new WebAssembly.Tag({ parameters: ["v128"] });
    const vvtag = new WebAssembly.Tag({ parameters: ["v128", "v128"] });
    const vitag = new WebAssembly.Tag({ parameters: ["v128", "i32"] });
    const instance = await instantiate(wat, { tags: {
        vtag: vtag,
        vvtag: vvtag,
        vitag: vitag
    } }, { exceptions: true, simd: true });
    assert.eq(instance.exports.testThrowV128(), 42);
    assert.eq(instance.exports.testThrowV128Pair(), 42);
    assert.eq(instance.exports.testThrowV128AndI32(), 42);
}

await assert.asyncTest(test());
