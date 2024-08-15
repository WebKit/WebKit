//@ requireOptions("--useWasmJITLessJSEntrypoint=1")
//  Debugging: jsc -m cc-lotsofints-to-int.js --useConcurrentJIT=0 --useBBQJIT=0 --useOMGJIT=0 --jitAllowList=nothing --useDFGJIT=0 --dumpDisassembly=0 --forceICFailure=1 --useWasmJITLessJSEntrypoint=1 --dumpDisassembly=0
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $test (export "test")
        (param $x0 i32)
        (param $x1 i32)
        (param $x2 i32)
        (param $x3 i32)
        (param $x4 i32)
        (param $x5 i32)
        (param $x6 i32)
        (param $x7 i32)
        (param $x8 i32)
        (param $x9 i32)
        (param $xa i32)
        (param $xb i32)
        (param $xc i32)
        (param $xd i32)
        (param $xe i32)
        (param $xf i32)
        (param $xg i32)

        (result i32)

        (i32.add (local.get $x0)
            (i32.add (local.get $x1)
                (i32.add (local.get $x2)
                    (i32.add (local.get $x3)
                        (i32.add (local.get $x4)
                            (i32.add (local.get $x5)
                                (i32.add (local.get $x6)
                                    (i32.add (local.get $x7)
                                        (i32.add (local.get $x8)
                                            (i32.add (local.get $x9)
                                                (i32.add (local.get $xa)
                                                    (i32.add (local.get $xb)
                                                        (i32.add (local.get $xc)
                                                            (i32.add (local.get $xd)
                                                                (i32.add (local.get $xe)
                                                                    (i32.add (local.get $xf) (local.get $xg)))))))))))))))))
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { test } = instance.exports

    let arr = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1];

    for (let i = 0; i < 32; ++i) {
        let next = test(...arr);
        assert.eq(next, arr.reduce((a, b) => a + b, 0));
        arr.shift();
        arr.push(next);
    }
}

await assert.asyncTest(test())
