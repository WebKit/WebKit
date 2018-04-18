(module
    (table $table (export "table") 4 anyfunc)
    (type $t0 (func (param i32 i32) (result i32)))
    (func $sum (export "sum") (type $t0) (param $p0 i32) (param $p1 i32) (result i32)
        get_local $p1
        get_local $p0
        i32.add)
    (global (export "answer") i32 i32.const 42)
    (global (export "answer1") f32 f32.const 0.5)
    (global (export "answer2") f64 f64.const 0.5)
    (global (export "answer3") f32 f32.const nan)
    (global (export "answer4") f64 f64.const nan)
    (elem (i32.const 0) $sum))
