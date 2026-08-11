(module
  (tag $tag)
  (func $throws
    throw $tag)
  (func $empty)
  (func (export "test") (result i32)
    call $empty
    try (result i32)
      call $throws
      (i32.const 1)
    catch_all
      (i32.const 2)
    end))
