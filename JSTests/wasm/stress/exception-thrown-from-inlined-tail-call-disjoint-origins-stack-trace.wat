(module
    (import "js" "jsThrow" (func $jsThrow (param i32)))

    (global $state (mut i32) (i32.const 0))
    
    ;; These should never be inlined / compiled to OMG, based on a function index range.
    (func $throw0
        (if (i32.eq (i32.const 0) (global.get $state))
            (then (call $jsThrow (i32.const 0)))
        )
    )

    (func $throw1
        (if (i32.eq (i32.const 1) (global.get $state))
            (then (call $jsThrow (i32.const 1)))
        )
    )

    (func $throw2
        (if (i32.eq (i32.const 2) (global.get $state))
            (then (call $jsThrow (i32.const 2)))
        )
    )

    (func $throw3
        (if (i32.eq (i32.const 3) (global.get $state))
            (then (call $jsThrow (i32.const 3)))
        )
    )

    (func $throw4
        (if (i32.eq (i32.const 4) (global.get $state))
            (then (call $jsThrow (i32.const 4)))
        )
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

    (func $a
        call $throw3
    )
    (func $b
        call $throw0
    )

    (func $do_tail_a
        call $throw4
        return_call $a
    )

    (func $do_tail_b
        call $throw1
        return_call $b
    )

    (func $test (export "test") (result i32)
        call $do_tail_a
        call $throw2
        call $do_tail_b
        (i32.const -1)
    )
)
