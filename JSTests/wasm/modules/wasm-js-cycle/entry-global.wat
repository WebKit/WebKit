(module
    (import "./global.js" "incrementGlobal" (func))
    (import "./global.js" "globalFromJS" (global $fromJS (mut i32)))
    (func (export "getFromJSGlobal") (result i32)
        (global.get $fromJS))
    (global (export "g") (mut i32) (i32.const 42)))
