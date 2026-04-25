(module
    (import "./sum.js" "sum" (func $sum (param i32 i32) (result i32)))
    (type $t0 (func (param i32) (result i32)))
    (func $addOne (export "addOne") (type $t0) (param $p0 i32) (result i32)
        i32.const 1
        get_local $p0
        call $sum))
