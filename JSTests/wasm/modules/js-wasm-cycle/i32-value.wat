(module
    (import "./entry-i32-value.js" "glob" (global $glob i32))
    (func (export "getGlob") (result i32)
        (global.get $glob)))
