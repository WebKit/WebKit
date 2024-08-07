//@ requireOptions("--useInterpretedJSEntryWrappers=1")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func $f0 (export "f0")
        (param $x0 i64) (param $x1 i64) (param $x2 i64) (param $x3 i64)
        (param $x4 i64) (param $x5 i64) (param $x6 i64) (param $x7 i64)
        (result i64)
        (i64.add (local.get $x0)
            (i64.add (local.get $x1)
                (i64.add (local.get $x2)
                    (i64.add (local.get $x3)
                        (i64.add (local.get $x4)
                            (i64.add (local.get $x5)
                                (i64.add (local.get $x6)
                                    (local.get $x7))))))))
    )

    (func $f1 (export "f1")
        (param $x0 i64) (param $x1 i64) (param $x2 i64) (param $x3 i64)
        (param $x4 i64) (param $x5 i64) (param $x6 i64) (param $x7 i64)
        (param $x8 i64) (param $x9 i64) (param $x10 i64) (param $x11 i64)
        (result i64)
        (i64.add (local.get $x0)
            (i64.add (local.get $x1)
                (i64.add (local.get $x2)
                    (i64.add (local.get $x3)
                        (i64.add (local.get $x4)
                            (i64.add (local.get $x5)
                                (i64.add (local.get $x6)
                                    (i64.add (local.get $x7)
                                        (i64.add (local.get $x8)
                                            (i64.add (local.get $x9)
                                                (i64.add (local.get $x10)
                                                (local.get $x11))))))))))))
    )

    (func $f2 (export "f2")
        (param $x0 i64) (param $x1 i64) (param $x2 i64) (param $x3 i64)
        (param $x4 i64) (param $x5 i64) (param $x6 i64) (param $x7 i64)
        (param $x8 i64) (param $x9 i64) (param $x10 i64) (param $x11 i64)
        (result i64)
        (i64.add (local.get $x0)
            (local.get $x11))
    )

    (func $f3 (export "f3")
        (param $x0 i64) (param $x1 i64) (param $x2 i64) (param $x3 i64)
        (param $x4 i64) (param $x5 i64) (param $x6 i64) (param $x7 i64)
        (param $x8 i64) (param $x9 i64) (param $x10 i64) (param $x11 i64)
        (i64.add (local.get $x0)
            (local.get $x11))
        drop
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true })
    const { f0, f1, f2, f3 } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        assert.eq(f0(1n, 2n, 3n, 4n, 5n, 6n, 7n, 8n), BigInt(1 + 2 + 3 + 4 + 5 + 6 + 7 + 8))
        assert.eq(f0(1n, 2n, 3n, 4n, 5n, 6n, 7n, 8n, 9n, 10n), BigInt(1 + 2 + 3 + 4 + 5 + 6 + 7 + 8))
        assert.throws(() => f0(1n, 2n, 3n, 4n, 5n), TypeError, "")
        assert.throws(() => f1(1n, 2n, 3n, 4n, 5n, 6n, 7n, 8n), TypeError, "")
        assert.eq(f1(1n, 2n, 3n, 4n, 5n, 6n, 7n, 8n, 9n, 10n, 11n, 12n), BigInt(1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12))
        assert.throws(() => f1(1n, 2n, 3n, 4n, 5n), TypeError, "")
        assert.eq(f1(1n, 2n, 3n, 4n, 5n, 6n, 7n, 8n, 9n, 10n, 11n, 12n, 13n, 14n, 15n), BigInt(1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12))
        assert.eq(f2(1n, 2n, 3n, 4n, 5n, 6n, 7n, 8n, 9n, 10n, 11n, 12n, 13n, 14n, 15n), BigInt(1 + 12))
        assert.eq(f3(1n, 2n, 3n, 4n, 5n, 6n, 7n, 8n, 9n, 10n, 11n, 12n, 13n, 14n, 15n), undefined)
    }
}

await assert.asyncTest(test())
