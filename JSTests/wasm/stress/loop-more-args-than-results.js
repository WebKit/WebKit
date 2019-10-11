import { instantiate } from "../wabt-wrapper.js";

let wat = `
(module
  (func (export "test") (param i32) (result i32)
    i32.const 0
    i32.const 1
    (loop (param i32 i32) (result i32)
      drop
    )
  )
)
`;

let instance = instantiate(wat);

if (instance.exports.test() !== 0)
    throw new Error();
