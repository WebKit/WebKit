;; wat2wasm JSTests/wasm/stress/exception-thrown-from-inlined-tail-call-disjoint-origins-no-inline-tail-ref.wat -o JSTests/wasm/stress/exception-thrown-from-inlined-tail-call-disjoint-origins-no-inline-tail-ref.wasm --enable-all --debug-names
(module
    (tag $tag0)
    (tag $tag1)
    (tag $tag1b)
    (tag $tag2)
    (tag $tag3)
    (tag $tag4)
    (tag $tag4b)

    (type $void_func (func))

    (global $state (mut i32) (i32.const 0))
    (global $func_a (mut (ref null $void_func)) (ref.null $void_func))
    (global $func_b (mut (ref null $void_func)) (ref.null $void_func))
    
    ;; These should never be inlined / compiled to OMG, based on a function index range.
    (func $throw0
        (if (i32.eq (i32.const 0) (global.get $state))
            (then (throw $tag0))
        )
    )

    (func $throw1
        (if (i32.eq (i32.const 1) (global.get $state))
            (then (throw $tag1))
        )
    )

    (func $throw2
        (if (i32.eq (i32.const 2) (global.get $state))
            (then (throw $tag2))
        )
    )

    (func $throw3
        (if (i32.eq (i32.const 3) (global.get $state))
            (then (throw $tag3))
        )
    )

    (func $throw4
        (if (i32.eq (i32.const 4) (global.get $state))
            (then (throw $tag4))
        )
    )

    (func $a
        call $throw3
    )
    (func $b
        call $throw0
    )

    ;; Filler to make the allowlist indices easier.
    (func)
    (func)
    (func)
    (func)
    (func)
    (func)
    (func)
    (func)
    (func)
    (func)
    (func)
    (func)
    (func)

    (func $inc_state (export "inc_state")
        global.get $state
        i32.const 1
        i32.add
        global.set $state
    )

    (func $init
        (ref.func $a)
        (global.set $func_a)
        (ref.func $b)
        (global.set $func_b)
    )

    (func $do_tail_a
        (try
            (do
                call $throw4
                global.get $func_a
                return_call_ref $void_func
            )
            (catch_all
                throw $tag4b
            )
        )
    )

    (func $do_tail_b
        (try
            (do
                call $throw1
                global.get $func_b
                return_call_ref $void_func
            )
            (catch_all
                throw $tag1b
            )
        )
    )

    (func $test (export "test") (result i32)
        (try (result i32)
            (do
                call $do_tail_a
                call $throw2
                call $do_tail_b
                (i32.const -1)
            )
            (catch $tag0 (i32.const 0))
            (catch $tag1 (i32.const -100))
            (catch $tag2 (i32.const 2))
            (catch $tag3 (i32.const 3))
            (catch $tag4 (i32.const -100))
            (catch $tag1b (i32.const 1))
            (catch $tag4b (i32.const 4))
        )
    )

    (elem declare func $a $b)
    (start $init)
)