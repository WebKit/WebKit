;; Tools/Scripts/run-jsc -e "'load(\"JSTests/wasm/gc/wast.js\"); writeFile(\"JSTests/wasm/stress/type-index-abstract-heap-types-subtype-validation-funcref-to-externref.wasm\", WebAssemblyText.encode(read(\"JSTests/wasm/stress/type-index-abstract-heap-types-subtype-validation-funcref-to-externref.wat\")))'"
(module
  (func (param funcref) (result)
    (drop (ref.cast externref (local.get 0)))))
