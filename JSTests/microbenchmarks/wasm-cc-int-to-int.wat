(module
    (type $sig_test (func (param i32) (result i32)))
    (table $t 1 funcref)
    (elem (i32.const 0) $test)

    (func $test (export "test") (param $x i32) (result i32)
        (i32.add (local.get $x) (i32.const 42))
    )

    (func (export "test_with_call") (param $x i32) (result i32)
        (i32.add (local.get $x) (call $test (i32.const 1337)))
    )

    (func (export "test_with_call_indirect") (param $x i32) (result i32)
        (local.get $x)
        (i32.const 98)
        (call_indirect $t (type $sig_test) (i32.const 0))
        i32.add
    )
)