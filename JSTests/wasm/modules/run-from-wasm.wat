(module
    (import "./run-from-wasm/check.js" "check" (func $check (param i32)))
    (type $t0 (func))
    (func $main (type $t0)
        i32.const 42
        call $check)
    (start $main))
