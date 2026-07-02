(module
    (import "./sum.wasm" "sum" (func $sum (param i32) (param i32) (result i32)))
    (type $t0 (func (result i32)))
    (func $return42 (export "return42") (type $t0) (result i32)
        i32.const 1
        i32.const 41
        call $sum))
