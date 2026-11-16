(module
    (import "test" "wasm:invalid" (func $invalid (result i32)))
    (func (export "test") (result i32)
        call $invalid))
