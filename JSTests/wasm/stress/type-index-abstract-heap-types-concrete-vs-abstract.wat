;; Tools/Scripts/run-jsc -e "'load(\"JSTests/wasm/gc/wast.js\"); writeFile(\"JSTests/wasm/stress/type-index-abstract-heap-types-concrete-vs-abstract.wasm\", WebAssemblyText.encode(read(\"JSTests/wasm/stress/type-index-abstract-heap-types-concrete-vs-abstract.wat\")))'"
(module
  (type $S (sub (struct (field i32))))
  (type $T (sub $S (struct (field i32) (field i64))))
  (func (export "asAny") (result anyref)
    (struct.new $T (i32.const 1) (i64.const 2)))
  (func (export "asStruct") (result structref)
    (struct.new $S (i32.const 3)))
  (func (export "castToS") (param anyref) (result (ref null $S))
    (ref.cast (ref null $S) (local.get 0)))
  (func (export "testIsS") (param anyref) (result i32)
    (ref.test (ref null $S) (local.get 0)))
)
