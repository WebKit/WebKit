import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

// If we compute using a signed division, then the results of the long computation inside the `if`s below
// will be a large negative number rather than a large positive one, and the functions will return 1.
let wat = `
(module
    (type (func (param f64) (result i32)))
    (func (export "test64") (type 0) (local i64) (local i32)
        (local.set 1 (i64.const -9999))
        (if (i64.le_s (i64.clz (i64.add (local.get 1) (i64.const 1))) (i64.sub (i64.div_u (local.get 1) (i64.const 2)) (i64.const 2)))
            (then (local.set 2 (i32.const 0)))
            (else (local.set 2 (i32.const 1)))
        )
        (local.get 2)
    )

    (type (func (param f32) (result i32)))
    (func (export "test32") (type 1) (local i32) (local i32)
        (local.set 1 (i32.const -9999))
        (if (i32.le_s (i32.clz (i32.add (local.get 1) (i32.const 1))) (i32.sub (i32.div_u (local.get 1) (i32.const 2)) (i32.const 2)))
            (then (local.set 2 (i32.const 0)))
            (else (local.set 2 (i32.const 1)))
        )
        (local.get 2)
    )
)
`;

async function test() {
  const instance = await instantiate(wat, {}, {});
  const {test64, test32} = instance.exports;
  for (let i = 0; i < 100; i++) {
    assert.eq(test64(10.0), 0);
    assert.eq(test32(10.0), 0);

    assert.eq(test64(-10.0), 0);
    assert.eq(test32(-10.0), 0);

    assert.eq(test64(1028.0), 0);
    assert.eq(test32(1028.0), 0);

    assert.eq(test64(1234.0), 0);
    assert.eq(test32(1234.0), 0);
  }
}

await assert.asyncTest(test());
