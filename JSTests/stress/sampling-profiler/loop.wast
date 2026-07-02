(module
  (memory 1)

  (func (export "loop") (param i32) (result i32)
    (local i32 i32)
    (local.set 1 (i32.const 1))
    (local.set 2 (i32.const 2))
    (block
      (loop
        (br_if 1 (i32.gt_u (local.get 2) (local.get 0)))
        (local.set 1 (i32.mul (local.get 1) (local.get 2)))
        (local.set 2 (i32.add (local.get 2) (i32.const 1)))
        (br 0)
      )
    )
    (local.get 1)
  )
)
