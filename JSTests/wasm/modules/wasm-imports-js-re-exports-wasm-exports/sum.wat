(module
    (type $t0 (func (param i32 i32) (result i32)))
    (func $sum (export "sum") (type $t0) (param $p0 i32) (param $p1 i32) (result i32)
        get_local $p1
        get_local $p0
        i32.add)
    (global (export "answer") i32 i32.const 42)
    (table $table (export "table") 4 funcref)
    (memory $memory (export "memory") 1 1)
    (data (i32.const 4) "\10\00\10\00")
    (elem (i32.const 0) $sum))
