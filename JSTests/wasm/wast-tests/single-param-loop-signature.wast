(module
  (func (export "test") (result i32)
    (local $x i32)
    (i32.const 1)
    (loop (param i32) (result i32)
      (i32.const 4)
      (i32.add)
      (local.tee $x)
      (local.get $x)
      (i32.const 10)
      (i32.lt_u)
      (br_if 0)
    )
  )
  (func (export "checkResult") (param i32) (result i32)
    get_local 0
    i32.const 13
    i32.eq
  )
)
