import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

let wat = `
(module
  (func (export "test") (param i64 i64 i64) (result i64)
    local.get 0
    local.get 1
    local.get 2
    i64.const 42
    i64.add
    i64.add
    i64.add
  )
)
`;

async function test() {
    let instance = await instantiate(wat);
    if (instance.exports.test(44n, 22n, -3n) !== 105n)
        throw new Error();
}

assert.asyncTest(test());
