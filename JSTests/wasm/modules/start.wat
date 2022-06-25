(module
    (global $g0 (mut i32) i32.const 0)
    (type $t0 (func))
    (func $increment (type $t0)
        get_global $g0
        i32.const 1
        i32.add
        set_global $g0)
    (start $increment)
    (type $t1 (func (result i32)))
    (func $get (export "get") (type $t1) (result i32)
        get_global $g0))
