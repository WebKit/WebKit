;; wat2wasm JSTests/wasm/stress/exception-thrown-from-inlined-tail-call-disjoint-origins-multiple-tail-calls-indirect-try-table.wat -o JSTests/wasm/stress/exception-thrown-from-inlined-tail-call-disjoint-origins-multiple-tail-calls-indirect-try-table.wasm --enable-all --debug-names
(module
    (tag $tag0)
    (tag $tag0b)
    (tag $tag1)
    (tag $tag1b)
    (tag $tag2)
    (tag $tag3)
    (tag $tag4)
    (tag $tag4b)
    (tag $tag5)
    (tag $tag5b)
    (tag $tag6)
    (tag $tag6b)
    (tag $tag7)
    (tag $tag8)
    (tag $tag9)

    (type $void_func (func))
    (table $funcs 2 funcref)

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

    (func $throw5
        (if (i32.eq (i32.const 5) (global.get $state))
            (then (throw $tag5))
        )
    )

    (func $throw6
        (if (i32.eq (i32.const 6) (global.get $state))
            (then (throw $tag6))
        )
    )

    (func $throw7
        (if (i32.eq (i32.const 7) (global.get $state))
            (then (throw $tag7))
        )
    )

    (func $throw8
        (if (i32.eq (i32.const 8) (global.get $state))
            (then (throw $tag8))
        )
    )

    (func $throw9
        (if (i32.eq (i32.const 9) (global.get $state))
            (then (throw $tag9))
        )
    )

    (func $a
        call $throw2
        call $throw3
    )
    (func $b
        call $throw7
        call $throw8
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
        (block $catch_tag0 (result exnref)
        (block $catch_tag1 (result exnref)
            (try_table 
                (catch_ref $tag0 $catch_tag0)
                (catch_ref $tag1 $catch_tag1)
                call $throw0
                (if (i32.and (i32.le_s (global.get $state) (i32.const 2)) (i32.ge_s (global.get $state) (i32.const 0)))
                    (then
                        call $throw1
                        i32.const 0
                        return_call_indirect (type $void_func)
                    )
                )
                i32.const 0
                return_call_indirect (type $void_func)
            )
            unreachable
        )
        ;; catch $tag1
        drop
        throw $tag1b
        )
        ;; catch $tag0
        drop
        throw $tag0b
    )

    (func $do_tail_b
        (block $catch_tag5 (result exnref)
        (block $catch_tag6 (result exnref)
            (try_table 
                (catch_ref $tag5 $catch_tag5)
                (catch_ref $tag6 $catch_tag6)
                call $throw5
                (if (i32.and (i32.le_s (global.get $state) (i32.const 7)) (i32.ge_s (global.get $state) (i32.const 5)))
                    (then
                        call $throw6
                        i32.const 1
                        return_call_indirect (type $void_func)
                    )
                )
                i32.const 1
                return_call_indirect (type $void_func)
            )
            unreachable
        )
        ;; catch $tag6
        drop
        throw $tag6b
        )
        ;; catch $tag5
        drop
        throw $tag5b
    )

    (func $test (export "test") (result i32)
        (block $catch_tag0 (result exnref)
        (block $catch_tag0b (result exnref)
        (block $catch_tag1 (result exnref)
        (block $catch_tag1b (result exnref)
        (block $catch_tag2 (result exnref)
        (block $catch_tag3 (result exnref)
        (block $catch_tag4 (result exnref)
        (block $catch_tag5 (result exnref)
        (block $catch_tag5b (result exnref)
        (block $catch_tag6 (result exnref)
        (block $catch_tag6b (result exnref)
        (block $catch_tag7 (result exnref)
        (block $catch_tag8 (result exnref)
        (block $catch_tag9 (result exnref)
            (try_table 
                (catch_ref $tag0 $catch_tag0)
                (catch_ref $tag0b $catch_tag0b)
                (catch_ref $tag1 $catch_tag1)
                (catch_ref $tag1b $catch_tag1b)
                (catch_ref $tag2 $catch_tag2)
                (catch_ref $tag3 $catch_tag3)
                (catch_ref $tag4 $catch_tag4)
                (catch_ref $tag5 $catch_tag5)
                (catch_ref $tag5b $catch_tag5b)
                (catch_ref $tag6 $catch_tag6)
                (catch_ref $tag6b $catch_tag6b)
                (catch_ref $tag7 $catch_tag7)
                (catch_ref $tag8 $catch_tag8)
                (catch_ref $tag9 $catch_tag9)
                call $do_tail_a
                call $throw4
                call $do_tail_b
                call $throw9
                (i32.const -1)
                return
            )
            unreachable
        )
        ;; catch $tag9
        drop
        (i32.const 9)
        return
        )
        ;; catch $tag8
        drop
        (i32.const 8)
        return
        )
        ;; catch $tag7
        drop
        (i32.const 7)
        return
        )
        ;; catch $tag6b
        drop
        (i32.const 6)
        return
        )
        ;; catch $tag6
        drop
        (i32.const -6)
        return
        )
        ;; catch $tag5b
        drop
        (i32.const 5)
        return
        )
        ;; catch $tag5
        drop
        (i32.const -5)
        return
        )
        ;; catch $tag4
        drop
        (i32.const 4)
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
        ;; catch $tag1b
        drop
        (i32.const 1)
        return
        )
        ;; catch $tag1
        drop
        (i32.const -1)
        return
        )
        ;; catch $tag0b
        drop
        (i32.const 0)
        return
        )
        ;; catch $tag0
        drop
        (i32.const -100)
    )

    (elem (i32.const 0) func $a $b)
)