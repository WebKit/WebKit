(module
  (global $g (mut i32) (i32.const 100))
  (func (export "set") (param i32)
    (global.set $g (local.get 0)))
  (func (export "get") (result i32)
    (global.get $g))
  (export "g" (global $g)))
