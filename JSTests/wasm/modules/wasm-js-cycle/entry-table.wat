(module
    (import "./table.js" "setTable" (func (param i32) (param externref)))
    (import "./table.js" "tableFromJS" (table $fromJS 10 externref))
    (func (export "getFromJSTable") (result externref)
        (table.get $fromJS (i32.const 0)))
    (table (export "t") 10 externref))
