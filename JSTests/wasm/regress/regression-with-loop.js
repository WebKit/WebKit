import * as assert from '../assert.js';
import { instantiate } from '../wabt-wrapper.js';

const instance = instantiate(`
(module
  (func $test (param f64) (result f64)
    f64.const 1.0
    f64.const 2.0

    local.get 0
    i64.reinterpret_f64
    i32.wrap_i64

    i32.const 4096
    i32.lt_u
    select
)
(export "test" (func $test)))
`);

for (var i = 0; i < 1e4; ++i) {
    assert.eq(instance.exports.test(0.1111111111111111111111111111111111), 2.0);
    assert.eq(instance.exports.test(0), 1.0);
    assert.eq(instance.exports.test(1), 1.0);
    assert.eq(instance.exports.test(2), 1.0);
    assert.eq(instance.exports.test(3), 1.0);
    assert.eq(instance.exports.test(0.5), 1.0);
    assert.eq(instance.exports.test(-0.5), 1.0);
    assert.eq(instance.exports.test(200000), 1.0);
    assert.eq(instance.exports.test(-200000), 1.0);
}
