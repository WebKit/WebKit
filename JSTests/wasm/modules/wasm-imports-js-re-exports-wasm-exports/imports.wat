(module
    (import "./re-export.js" "sum" (func $sum (param i32 i32) (result i32)))
    (import "./re-export.js" "answer" (global i32))
    (import "./re-export.js" "table" (table $table 4 anyfunc))
    (export "table" (table $table))
    (type $t0 (func (param i32) (result i32)))
    (func $addOne (export "addOne") (type $t0) (param $p0 i32) (result i32)
        i32.const 1
        get_local $p0
        call $sum)
    (type $t1 (func (result i32)))
    (func $getAnswer (export "getAnswer") (type $t1) (result i32)
        get_global 0)
    (elem (i32.const 1) $addOne))
