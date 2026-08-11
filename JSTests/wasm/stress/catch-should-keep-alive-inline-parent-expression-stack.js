import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (type $outer-type (func (param funcref)))
    (type $inner-type (func (param externref)))
    (import "m" "x" (global $x (mut funcref)))

    (func $outer (type $outer-type) (param funcref)
      try
        ref.null extern
        call $inner
        br 1
      catch_all
        return
      end
    )

    (func $inner (type $inner-type) (param externref)
      ref.null func
      global.get $x
      call $outer
      global.set $x

      i32.const 0
      global.get $x
      table.set $func-table
    )

    (table $func-table 8 funcref)
    (export "outer" (func $outer))
)
`;

async function test() {
    let globalX = new WebAssembly.Global({value: 'anyfunc', mutable: true}, null);
    const instance = await instantiate(wat, { m: { x: globalX } }, { exceptions: true });
    const { outer } = instance.exports;
    for (let i = 0; i < 10; i++) {
        try {
            let result = outer(null, {}, {});
            throw "FAILED";
        } catch (e) {
            let exception = e;
            if (exception != "RangeError: Maximum call stack size exceeded.")
                throw "FAILED";
        }
    }
}

assert.asyncTest(test());
