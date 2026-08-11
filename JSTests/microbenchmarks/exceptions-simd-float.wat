;; wat2wasm exceptions-simd-float.wat -o exceptions-simd-float.wasm --enable-all
(module
    (import "m" "tag" (tag $e-i32 (param i32)))

    (func $ident (param $i f32) (result f32)
        (local.get $i))

    (func $test_throw (export "test_throw") (param $sz i32)
        (local $i i32)
        (local.set $i (i32.const 0))

        (loop $loop
            local.get $i
            i32.const 1
            i32.add
            local.set $i

            local.get $i
            local.get $sz
            (if (i32.eq) (then (throw $e-i32 (local.get $i))))

            local.get $i
            i32.const 1000
            i32.lt_s
            br_if $loop)
    )

    (func $test_catch (export "test_catch") (result f32)
        (f32.const 1337)
        (call $ident)
        (try
            (do
                (call $test_throw (i32.const 500)))
        (catch $e-i32
            (i32.const 500)
            (if (i32.ne) (then (unreachable)))
        ))

        (f32.add (f32.const 0.0)))
)