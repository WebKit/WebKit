import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (func $nop-for-flush)
    (func (export "test") (param f64 i32) (result f64)
        local.get 0
        call $nop-for-flush
        local.get 1
        i32.eqz
        if (param f64) (result f64)
            (f64.add (f64.const 1))
        end
    )
)
`

let instance = await instantiate(wat);
assert.eq(instance.exports.test(2.3, 0), 3.3);
assert.eq(instance.exports.test(2.3, 1), 2.3);