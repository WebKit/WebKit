(module
    (import "./shared-memory.js" "memory" (memory $m 1 10 shared))
    (func (export "getMem") (param i32) (result i32)
        (i32.load8_u (local.get 0))))
