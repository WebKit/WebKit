import { instantiate } from "../wabt-wrapper.js";

let wat = `
(module
  (func (export "test") (param i32) (result i32) (local $l0 i32)
    local.get 0
    i32.const 42
    i32.add
    (loop $L0 (result i32)
        local.get $l0
        i32.const 1
        i32.add
        local.set $l0
        local.get $l0
        i32.const 100000
        i32.lt_u
        br_if 0
        i32.const 44
    )
    i32.add
  )
)
`;

let instance = instantiate(wat);
if (instance.exports.test(44) !== 130)
    throw new Error();
