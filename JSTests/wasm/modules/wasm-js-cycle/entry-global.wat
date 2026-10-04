(module
    (import "./global.js" "incrementGlobal" (func))
    (import "./global.js" "globalFromJS" (global $fromJS (mut i32)))
    (func (export "getFromJSGlobal") (result i32)
        (global.get $fromJS))
    (global $g (export "g") (mut i32) (i32.const 42))
    (func (export "increment")
        (global.set $g (i32.add (global.get $g) (i32.const 1)))))
