(module
    (import "test" "wasm:invalid" (func $invalid (result i32)))
    (func (export "wasm:bad") (result i32)
        i32.const 42))
