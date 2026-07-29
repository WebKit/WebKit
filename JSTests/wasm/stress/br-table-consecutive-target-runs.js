//@ $skipModes << "wasm-no-jit".to_sym
//@ $skipModes << "wasm-no-wasm-jit".to_sym
//@ requireOptions("--useOMGJIT=0", "--useWasmIPInt=0")
// https://bugs.webkit.org/show_bug.cgi?id=277686
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
  (func (export "smallRuns") (param i32) (result i32)
    (block $outer (result i32)
      (block $a (result i32)
        (block $b (result i32)
          (block $c (result i32)
            (i32.const 0)
            (local.get 0)
            (br_table $c $c $c $b $b $a $outer)
          )
          (drop)
          (i32.const 10)
          (return)
        )
        (drop)
        (i32.const 20)
        (return)
      )
      (drop)
      (i32.const 30)
      (return)
    )
    (drop)
    (i32.const -1)
  )

  (func (export "manyIndicesFewRuns") (param i32) (result i32)
    (block $outer (result i32)
      (block $r1 (result i32)
        (block $r2 (result i32)
          (block $r3 (result i32)
            (i32.const 0)
            (local.get 0)
            (br_table
              $r1 $r1 $r1 $r1 $r1 $r1 $r1 $r1 $r1 $r1
              $r2 $r2 $r2 $r2 $r2 $r2 $r2 $r2 $r2 $r2
              $r3 $r3 $r3 $r3 $r3 $r3 $r3 $r3 $r3 $r3
              $outer)
          )
          (drop)
          (i32.const 3)
          (return)
        )
        (drop)
        (i32.const 2)
        (return)
      )
      (drop)
      (i32.const 1)
      (return)
    )
    (drop)
    (i32.const 0)
  )

  (func (export "allSame") (param i32) (result i32)
    (block $outer (result i32)
      (block $only (result i32)
        (i32.const 0)
        (local.get 0)
        (br_table $only $only $only $only $only $outer)
      )
      (drop)
      (i32.const 42)
      (return)
    )
    (drop)
    (i32.const -1)
  )

  (func (export "alternating") (param i32) (result i32)
    (block $outer (result i32)
      (block $a (result i32)
        (block $b (result i32)
          (i32.const 0)
          (local.get 0)
          (br_table $a $b $a $b $a $b $outer)
        )
        (drop)
        (i32.const 2)
        (return)
      )
      (drop)
      (i32.const 1)
      (return)
    )
    (drop)
    (i32.const -1)
  )
)
`;

async function test() {
    const instance = await instantiate(wat, {}, {});
    const { smallRuns, manyIndicesFewRuns, allSame, alternating } = instance.exports;

    for (let n = 0; n < wasmTestLoopCount; ++n) {
        for (let i = 0; i < 3; ++i)
            assert.eq(smallRuns(i), 10);
        for (let i = 3; i < 5; ++i)
            assert.eq(smallRuns(i), 20);
        assert.eq(smallRuns(5), 30);
        assert.eq(smallRuns(6), -1);
        assert.eq(smallRuns(7), -1);
        assert.eq(smallRuns(-1), -1);
        assert.eq(smallRuns(-100), -1);
        assert.eq(smallRuns(0x7fffffff), -1);
        assert.eq(smallRuns(-0x80000000), -1);

        for (let i = 0; i < 10; ++i)
            assert.eq(manyIndicesFewRuns(i), 1);
        for (let i = 10; i < 20; ++i)
            assert.eq(manyIndicesFewRuns(i), 2);
        for (let i = 20; i < 30; ++i)
            assert.eq(manyIndicesFewRuns(i), 3);
        assert.eq(manyIndicesFewRuns(30), 0);
        assert.eq(manyIndicesFewRuns(31), 0);
        assert.eq(manyIndicesFewRuns(-1), 0);
        assert.eq(manyIndicesFewRuns(1000), 0);

        for (let i = 0; i < 5; ++i)
            assert.eq(allSame(i), 42);
        assert.eq(allSame(5), -1);
        assert.eq(allSame(-1), -1);

        for (let i = 0; i < 6; ++i)
            assert.eq(alternating(i), (i % 2) + 1);
        assert.eq(alternating(6), -1);
        assert.eq(alternating(-1), -1);
    }
}

await assert.asyncTest(test());
