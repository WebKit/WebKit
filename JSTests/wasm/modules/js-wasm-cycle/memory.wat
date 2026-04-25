(module
    (import "./entry-memory.js" "m" (memory $m 10))
    (func (export "f") (result i32)
        (memory.size)))
