import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
  (table $table 0 externref)
  (func (export "size") (result i32) (table.size $table))
  (func (export "grow") (param $sz i32) (result i32) 
    (table.grow $table (ref.null extern) (local.get $sz))
  )
)
`;
async function test() {
    const instance = await instantiate(wat, {}, {reference_types: true});
    const {size, grow} = instance.exports;
    assert.eq(size(), 0);
    assert.eq(grow(42), 0);
    assert.eq(size(), 42);
}

assert.asyncTest(test());
