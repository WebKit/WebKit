(module
    (import "./entry-i32-global.js" "glob" (global i32))
    (global (export "glob2") i32 (i32.const 42)))
