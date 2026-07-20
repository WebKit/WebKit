(module
  (tag $tag)
  (tag $tag2)
  (tag $tag3)
  (func $throws
    (throw $tag))
  (func $callthrows
    (call $throws))
  (func $callcallthrows
    (call $callthrows))
  (func $f0 (result i32)
    (i32.const 27))
  (func $f1
    (try
      (do
        (try
          (do
            (return_call $callcallthrows))
          (catch_all
            unreachable)))
      (catch $tag
        unreachable)))
  (func $f2
    (call $f1))
  (func $f3
    (call $f2))
  (func $f4
    (try
      (do
        (return_call $f2)) ;; $f2 throws and should not be handled here either.
      (catch $tag
        (throw $tag2)))
    (throw $tag3))
  (func $f5 (result i32)
      (call $f4)
      (i32.const 11))
  (func (export "test") (result i32)
    (try (result i32)
      (do
        (i32.add
          (call $f0)
          (call $f5)))
      (catch $tag
        (i32.const 2))
      (catch $tag2
        (i32.const 3))
      (catch $tag3
        (i32.const 4))
      (catch_all
        (i32.const 5)))))
