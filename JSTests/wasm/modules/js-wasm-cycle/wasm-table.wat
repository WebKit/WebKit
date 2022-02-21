(module
    (import "./entry-wasm-table.js" "f" (func $f (param i32) (result i32)))
    (table (export "t") 10 funcref))
