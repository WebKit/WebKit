(module
    (import "./entry-function.js" "f" (func $f (param i32) (result i32)))
    (func (export "f2") (param i32) (result i32)
        (i32.const 42)
        (call $f)))
