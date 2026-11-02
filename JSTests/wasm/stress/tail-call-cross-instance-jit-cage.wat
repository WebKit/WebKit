;; Tools/Scripts/run-jsc -e "'load(\"JSTests/wasm/libwabt.js\"); WabtModule().then(w => writeFile(\"JSTests/wasm/stress/tail-call-cross-instance-jit-cage.wasm\", w.parseWat(\"x\", read(\"JSTests/wasm/stress/tail-call-cross-instance-jit-cage.wat\"), { tail_call: true }).toBinary({}).buffer))'"
(module
    (import "m" "g" (func $g (result i32)))
    (func (export "f") (result i32) (return_call $g))
    (func (export "g") (result i32) (i32.const 42))
)
