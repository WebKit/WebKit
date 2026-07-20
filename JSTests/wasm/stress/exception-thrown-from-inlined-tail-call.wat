(module
  (tag $tag)
  (func $throws
    throw $tag)
  (func $f (export "f")
    try
      return_call $throws
    catch_all
      unreachable
    end
  )
  (func (export "test") (result i32)
    try (result i32)
      call $f
      (i32.const 1)
    catch $tag
      (i32.const 2)
    catch_all
      (i32.const 3)
    end
  )
)
