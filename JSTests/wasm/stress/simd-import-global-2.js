//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if $architecture != "arm64" && $architecture != "x86_64"
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

async function test() {
    assert.throws(() => new WebAssembly.Global({ value: "v128" }), TypeError, "WebAssembly.Global expects its 'value' field to be the string 'i32', 'i64', 'f32', 'f64', 'anyfunc', 'funcref', or 'externref'")
    const exportV128 = await instantiate(`
    (module
        (global (;0;) v128 v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000)
        (export "global" (global 0))
    )`, { }, { simd: true })

    if (typeof WebAssembly.Global.prototype.type !== 'undefined')
        assert.eq(exportV128.exports.global.type().value, "v128")
    assert.throws(() => exportV128.exports.global.value, WebAssembly.RuntimeError, "Cannot get value of v128 global (evaluating 'exportV128.exports.global.value')")

    await instantiate(`
    (module
        (import "x" "v128" (global v128))
    )`, { x: { v128: exportV128.exports.global } }, { simd: true })

    const exportMutV128 = await instantiate(`
    (module
        (global (;0;) (mut v128) v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000)
        (export "global" (global 0))
    )`, { }, { simd: true })
    await instantiate(`
    (module
        (import "x" "v128" (global (mut v128)))
    )`, { x: { v128: exportMutV128.exports.global } }, { simd: true })

    await assert.throwsAsync(instantiate(`
    (module
        (import "x" "v128" (global (mut v128)))
    )`, { x: { v128: exportV128.exports.global } }, { simd: true }), WebAssembly.LinkError, "imported global x:v128 must be a same mutability (evaluating 'new WebAssembly.Instance(module, imports)')")

    await assert.throwsAsync(instantiate(`
    (module
        (import "x" "v128" (global i64))
    )`, { x: { v128: exportV128.exports.global } }, { simd: true }), WebAssembly.LinkError, "imported global x:v128 must be a same type (evaluating 'new WebAssembly.Instance(module, imports)')")

    const exportI64 = await instantiate(`
    (module
        (global i64 i64.const 0)
        (export "global" (global 0))
    )`, { }, { simd: true })
    await assert.throwsAsync(instantiate(`
    (module
        (import "x" "v128" (global v128))
    )`, { x: { v128: exportI64.exports.global } }, { simd: true }), WebAssembly.LinkError, "imported global x:v128 must be a same type (evaluating 'new WebAssembly.Instance(module, imports)')")

    const exportI64Mut = await instantiate(`
    (module
        (global i64 i64.const 0)
        (export "global" (global 0))
    )`, { }, { simd: true })
    await assert.throwsAsync(instantiate(`
    (module
        (import "x" "v128" (global (mut v128)))
    )`, { x: { v128: exportI64Mut.exports.global } }, { simd: true }), WebAssembly.LinkError, "imported global x:v128 must be a same type (evaluating 'new WebAssembly.Instance(module, imports)')")
}

assert.asyncTest(test())