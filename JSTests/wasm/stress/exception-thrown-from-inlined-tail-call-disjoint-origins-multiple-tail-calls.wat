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
        (try
            (do
                call $throw0
                (if (i32.and (i32.le_s (global.get $state) (i32.const 2)) (i32.ge_s (global.get $state) (i32.const 0)))
                    (then
                        call $throw1
                        return_call $a
                    )
                )
                return_call $a
            )
            (catch $tag0
                throw $tag0b
            )
            (catch $tag1
                throw $tag1b
            )
        )
    )

    (func $do_tail_b
        (try
            (do
                call $throw5
                (if (i32.and (i32.le_s (global.get $state) (i32.const 7)) (i32.ge_s (global.get $state) (i32.const 5)))
                    (then
                        call $throw6
                        return_call $b
                    )
                )
                return_call $b
            )
            (catch $tag5
                throw $tag5b
            )
            (catch $tag6
                throw $tag6b
            )
        )
    )

    (func $test (export "test") (result i32)
        (try (result i32)
            (do
                call $do_tail_a
                call $throw4
                call $do_tail_b
                call $throw9
                (i32.const -1)
            )
            (catch $tag0 (i32.const -100))
            (catch $tag0b (i32.const 0))
            (catch $tag1 (i32.const -1))
            (catch $tag1b (i32.const 1))
            (catch $tag2 (i32.const 2))
            (catch $tag3 (i32.const 3))
            (catch $tag4 (i32.const 4))
            (catch $tag5 (i32.const -5))
            (catch $tag5b (i32.const 5))
            (catch $tag6 (i32.const -6))
            (catch $tag6b (i32.const 6))
            (catch $tag7 (i32.const 7))
            (catch $tag8 (i32.const 8))
            (catch $tag9 (i32.const 9))
        )
    )
)
