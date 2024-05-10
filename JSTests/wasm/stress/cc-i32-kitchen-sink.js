//@ requireOptions("--useIPIntWrappers=1")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $f0 (export "f0")
        (param $x0 i32) (param $x1 i32) (param $x2 i32) (param $x3 i32)
        (param $x4 i32) (param $x5 i32) (param $x6 i32) (param $x7 i32)
        (result i32)
        (i32.add (local.get $x0)
            (i32.add (local.get $x1)
                (i32.add (local.get $x2)
                    (i32.add (local.get $x3)
                        (i32.add (local.get $x4)
                            (i32.add (local.get $x5)
                                (i32.add (local.get $x6)
                                    (local.get $x7))))))))
    )

    (func $f1 (export "f1")
        (param $x0 i32) (param $x1 i32) (param $x2 i32) (param $x3 i32)
        (param $x4 i32) (param $x5 i32) (param $x6 i32) (param $x7 i32)
        (param $x8 i32) (param $x9 i32) (param $x10 i32) (param $x11 i32)
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
                                                (i32.add (local.get $x10)
                                                (local.get $x11))))))))))))
    )

    (func $f2 (export "f2")
        (param $x0 i32) (param $x1 i32) (param $x2 i32) (param $x3 i32)
        (param $x4 i32) (param $x5 i32) (param $x6 i32) (param $x7 i32)
        (param $x8 i32) (param $x9 i32) (param $x10 i32) (param $x11 i32)
        (result i32)
        (i32.add (local.get $x0)
            (local.get $x11))
    )

    (func $f3 (export "f3")
        (param $x0 i32) (param $x1 i32) (param $x2 i32) (param $x3 i32)
        (param $x4 i32) (param $x5 i32) (param $x6 i32) (param $x7 i32)
        (param $x8 i32) (param $x9 i32) (param $x10 i32) (param $x11 i32)
        (i32.add (local.get $x0)
            (local.get $x11))
        drop
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { f0, f1, f2, f3 } = instance.exports

    for (let i = 0; i < 10000000; ++i) {
        assert.eq(f0(1, 2, 3, 4, 5, 6, 7, 8), 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8)
        assert.eq(f0(1, 2, 3, 4, 5, 6, 7, 8, 9, 10), 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8)
        assert.eq(f0(1, 2, 3, 4, 5), 1 + 2 + 3 + 4 + 5)
        assert.eq(f1(1, 2, 3, 4, 5, 6, 7, 8), 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8)
        assert.eq(f1(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12), 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12)
        assert.eq(f1(1, 2, 3, 4, 5), 1 + 2 + 3 + 4 + 5)
        assert.eq(f1(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15), 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12)
        assert.eq(f2(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15), 1 + 12)
        assert.eq(f3(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15), undefined)
    }
}

await assert.asyncTest(test())
