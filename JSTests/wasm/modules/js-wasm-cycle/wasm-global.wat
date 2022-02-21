(module
    (import "./entry-wasm-global.js" "f" (func $f (param i32) (result i32)))
    (global (export "g") i32 (i32.const 42)))
