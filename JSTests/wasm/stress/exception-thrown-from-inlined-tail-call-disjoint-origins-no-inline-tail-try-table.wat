;; wat2wasm JSTests/wasm/stress/exception-thrown-from-inlined-tail-call-disjoint-origins-no-inline-tail-try-table.wat -o JSTests/wasm/stress/exception-thrown-from-inlined-tail-call-disjoint-origins-no-inline-tail-try-table.wasm --enable-all --debug-names
(module
    (tag $tag0)
    (tag $tag1)
    (tag $tag1b)
    (tag $tag2)
    (tag $tag3)
    (tag $tag4)
    (tag $tag4b)

    (global $state (mut i32) (i32.const 0))
    
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

    (func $do_tail_a
        (block $catch_all (result exnref)
            (try_table (catch_all_ref $catch_all)
                call $throw4
                return_call $a
            )
            unreachable
        )
        ;; If we get here, we caught an exception
        drop
        throw $tag4b
    )

    (func $do_tail_b
        (block $catch_all (result exnref)
            (try_table (catch_all_ref $catch_all)
                call $throw1
                return_call $b
            )
            unreachable
        )
        ;; If we get here, we caught an exception
        drop
        throw $tag1b
    )

    (func $test (export "test") (result i32)
        (block $catch_tag0 (result exnref)
        (block $catch_tag1 (result exnref)
        (block $catch_tag2 (result exnref)
        (block $catch_tag3 (result exnref)
        (block $catch_tag4 (result exnref)
        (block $catch_tag1b (result exnref)
        (block $catch_tag4b (result exnref)
            (try_table 
                (catch_ref $tag0 $catch_tag0)
                (catch_ref $tag1 $catch_tag1)
                (catch_ref $tag2 $catch_tag2)
                (catch_ref $tag3 $catch_tag3)
                (catch_ref $tag4 $catch_tag4)
                (catch_ref $tag1b $catch_tag1b)
                (catch_ref $tag4b $catch_tag4b)
                call $do_tail_a
                call $throw2
                call $do_tail_b
                (i32.const -1)
                return
            )
            unreachable
        )
        ;; catch $tag4b
        drop
        (i32.const 4)
        return
        )
        ;; catch $tag1b
        drop
        (i32.const 1)
        return
        )
        ;; catch $tag4
        drop
        (i32.const -100)
        return
        )
        ;; catch $tag3
        drop
        (i32.const 3)
        return
        )
        ;; catch $tag2
        drop
        (i32.const 2)
        return
        )
        ;; catch $tag1
        drop
        (i32.const -100)
        return
        )
        ;; catch $tag0
        drop
        (i32.const 0)
    )
)