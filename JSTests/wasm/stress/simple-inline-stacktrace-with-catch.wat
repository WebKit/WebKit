;; wat2wasm --enable-exceptions simple-inline-stacktrace-with-catch.wat -o simple-inline-stacktrace-with-catch.wasm
(module
  (import "a" "doThrow" (func $throw))
  ;; Make fn indices obvious
  (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func))
  (tag $e (export "tag") (param i32))
  (memory (export "mem") 1)

  (func $depth-5
    (call $throw)
    i32.const 0
    (i32.add (i32.load (i32.const 0)) (i32.const 1))
    i32.store
    (call $throw)
  )
  (func $depth-4
    try
      call $depth-5
    end
  )
  (func $depth-3
    (call $depth-4)
  )
  (func $depth-2
    (call $throw)
    (call $depth-3)
    (call $throw)
  )
  (func $depth-1
    try (block
      (call $throw)
      (call $depth-2)
      (call $throw))
    catch_all (block
      (call $throw)
      rethrow 1)
    end
    (call $throw)
    (call $depth-2)
  )
  (func $main (export "main")
    (local $i i32)
    (call $throw)
    (call $depth-1)
    (call $throw)
  )
)
