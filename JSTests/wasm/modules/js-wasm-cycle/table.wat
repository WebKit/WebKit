(module
    (import "./entry-table.js" "t" (table $t 5 funcref))
    (func (export "f") (result i32)
        (table.get $t (i32.const 0))
        (ref.is_null)))
