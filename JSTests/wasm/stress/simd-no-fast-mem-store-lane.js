//@ requireOptions("--useWebAssemblySIMD=1", "--useWebAssemblyFastMemory=0")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

 let wat = `
 (module
    (memory 1)

    (data (i32.const 0) "` + String.raw`\00\01\02\03\04\05\06\07\08\09\0A\0B\0C\0D\0E\0F` + `")
    (data (i32.const 65520) "` + String.raw`\10\11\12\13\14\15\16\17\18\19\1A\1B\1C\1D\1E\1F` + `")

    (func (export "v128_store8_lane") (result i32)
        (v128.store8_lane 0 (i32.const 65535) (v128.const i8x16 7 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))
        (i32.load8_u (i32.const 65535))
    )

 )
 `

 async function test() {
    const instance = await instantiate(wat, { imports: { } }, { simd: true })

    const {
        v128_store8_lane,
    } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(v128_store8_lane(), 7)
    }
 }

 assert.asyncTest(test())
