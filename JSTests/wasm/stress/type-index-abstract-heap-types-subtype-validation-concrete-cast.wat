;; Tools/Scripts/run-jsc -e "'load(\"JSTests/wasm/gc/wast.js\"); writeFile(\"JSTests/wasm/stress/type-index-abstract-heap-types-subtype-validation-concrete-cast.wasm\", WebAssemblyText.encode(read(\"JSTests/wasm/stress/type-index-abstract-heap-types-subtype-validation-concrete-cast.wat\")))'"
(module
  (type $S (struct))
  (func (param (ref null $S)) (result)
    (drop (local.get 0)))
  (func (param anyref) (result)
    (call 0 (ref.cast (ref null $S) (local.get 0)))))
