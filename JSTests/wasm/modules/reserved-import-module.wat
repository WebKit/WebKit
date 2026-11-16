(module
    (import "wasm-js:invalid" "test" (func $invalid (result i32)))
    (func (export "test") (result i32)
        call $invalid))
