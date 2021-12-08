(module
    (import "./memory.js" "memory" (memory $m 1))
    (func (export "getMem") (param i32) (result i32)
        (i32.load8_u (local.get 0))))
