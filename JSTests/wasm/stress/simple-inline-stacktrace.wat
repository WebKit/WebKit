;; wat2wasm --enable-exceptions simple-inline-stacktrace.wat -o simple-inline-stacktrace.wasm --debug-names
(module
  (import "a" "doThrow" (func $throw))
  ;; Make fn indices obvious
  (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func)) (import "a" "doThrow" (func))
  (tag $e (export "tag") (param i32))
  (memory (export "mem") 1)

  (func $g
    i32.const 0
    (i32.add (i32.load (i32.const 0)) (i32.const 1))
    i32.store

    call $throw
  )
  (func $a
    (local $i i32)
    (local $sum i32)

    (call $b)
  )
  (func $b
    (local $i i32)
    (local i32)
    (local i32)

    (call $c)
  )
  (func $c
    (local $i i32)
    (local $sum i32)
    (local i32)
    (local i32)

    (call $d)
  )
  (func $d
    (local $i i32)
    (local $sum i32)
    (local i32)
    (local i32)
    (local i32)

    (call $e)
  )
  (func $e
    (local $i i32)
    (local $sum i32)
    (local i32)
    (local i32)
    (local i32)
    (local i32)

    (call $f)
  )
  (func $f
    (local $i i32)
    (local $sum i32)
    (local i32)
    (local i32)
    (local i32)
    (local i32)
    (local i32)

    (call $g)
  )
  (func $main (export "main")
    (local $i i32)

    (call $a)
  )
)
