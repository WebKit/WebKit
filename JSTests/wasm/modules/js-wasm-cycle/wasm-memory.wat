(module
    (import "./entry-wasm-memory.js" "f" (func $f (param i32) (result i32)))
    (memory (export "m") 10))
