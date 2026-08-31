(module
    (import "wasm:js/string-constants" "wasm:hello" (global $s externref))
    (export "s" (global $s)))
