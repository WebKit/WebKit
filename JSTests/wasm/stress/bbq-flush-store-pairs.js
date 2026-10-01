import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
  (func $id (import "m" "id") (param i64) (result i64))
  (func $idf (import "m" "idf") (param f64) (result f64))
  (tag $e)
  (func (export "sumI64") (param i64 i64 i64 i64 i64 i64) (result i64)
    (drop (call $id (i64.const 0)))
    (i64.add (local.get 0) (local.get 1))
    (i64.add (local.get 2))
    (i64.add (local.get 3))
    (i64.add (local.get 4))
    (i64.add (local.get 5)))
  (func (export "sumF64") (param f64 f64 f64 f64) (result f64)
    (drop (call $idf (f64.const 0)))
    (f64.add (local.get 0) (local.get 1))
    (f64.add (local.get 2))
    (f64.add (local.get 3)))
  (func (export "sumAfterCatch") (param i64 i64 i64 i64) (result i64)
    (try
      (do (throw $e))
      (catch $e))
    (i64.add (local.get 0) (local.get 1))
    (i64.add (local.get 2))
    (i64.add (local.get 3)))
)
`

async function test() {
    const { sumI64, sumF64, sumAfterCatch } = (await instantiate(wat, { m: { id: x => x, idf: x => x } }, { exceptions: true })).exports
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        assert.eq(sumI64(1n, 2n, 3n, 4n, 5n, 6n), 21n)
        assert.eq(sumF64(1, 2, 3, 4), 10)
        assert.eq(sumAfterCatch(1n, 2n, 3n, 4n), 10n)
    }
}

await assert.asyncTest(test())
