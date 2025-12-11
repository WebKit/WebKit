;; wat2wasm exception-thrown-from-inlined-tail-call-call-tree-with-stacktrace.wat  --enable-exceptions  --enable-tail-call --debug-names

(module
  (import "m" "jsThrow" (func $jsThrow))
  (tag $tagA)
  (tag $tagB)
  (func $maybeA (param i32)
    ;; If the parameter is not zero then throw,
    ;; else return normally.
    (if 
      (local.get 0)
      (then
        (throw $tagA))))
  (func $maybe_jsThrow (param i32)
    ;; If the parameter is not zero then call $jsThrow,
    ;; else return normally.
    (if 
      (local.get 0)
      (then
        (call $jsThrow))))
  (func $tail_calls_maybeA (param i32)
    (try
      (do
        (return_call $maybeA
          (local.get 0))
        unreachable)
      (catch_all
        unreachable)))
  (func $tail_calls_maybe_jsThrow (param i32)
    (return_call $maybe_jsThrow
      (local.get 0))
    unreachable)
  (func $tail_calls_maybeB (param $ifA i32)
                           (param $ifB i32)
                           (param $if_jsThrow i32)
    (try
      (do
        (return_call $maybeB
          (local.get $ifA)
          (local.get $ifB)
          (local.get $if_jsThrow))
        unreachable)
      (catch_all
        unreachable)))
  (func $maybeB (param $ifA i32)
                (param $ifB i32)
                (param $if_jsThrow i32)
    (try ;; Catches any exception and rethrows from $maybeB. 
      (do
        (call $tail_calls_maybeA
          (local.get $ifA))
        (if
          (local.get $ifB)
          (then
            (throw $tagB))
          (else
            (call $tail_calls_maybe_jsThrow
              (local.get $if_jsThrow)))))
      (catch_all
        rethrow 0)))
  (func $test (export "test")  (param $ifA i32)
                         (param $ifB i32)
                         (param $if_jsThrow i32)
                         (result i32)
    (try (result i32)
      (do
        (call $tail_calls_maybeB
          (local.get $ifA)
          (local.get $ifB)
          (local.get $if_jsThrow))
        (i32.const 1)) ;; If no tail call threw, return 1.
      (catch $tagA
        (i32.const 2))
      (catch $tagB
        (i32.const 3))))) ;;If $jsThrow was called, the JS Error escapes to get the stack trace.
