(module
    (import "./table.js" "table" (table $t 10 externref))
    (func (export "getElem") (result externref)
        (table.get $t (i32.const 0))))
