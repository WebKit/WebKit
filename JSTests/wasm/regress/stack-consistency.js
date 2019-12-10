import { instantiate } from "../wabt-wrapper.js";

instantiate(`
(module
  (func $foo (result i32 i32) unreachable)
  (func (param i32) (result i32 i32 i32)
        (local.get 0)
        (loop (param i32) (result i32 i32 i32)
              call $foo)
        )
  )
`);
