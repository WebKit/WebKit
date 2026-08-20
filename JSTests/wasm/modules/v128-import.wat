(module
    (import "./v128-global.wasm" "v" (global v128))
    (func (export "ok") (result i32)
        i32.const 1))
