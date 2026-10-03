;; Tools/Scripts/run-jsc -e "'load(\"JSTests/wasm/gc/wast.js\"); writeFile(\"JSTests/wasm/stress/type-index-abstract-heap-types-subtype-validation-externref-to-anyref.wasm\", WebAssemblyText.encode(read(\"JSTests/wasm/stress/type-index-abstract-heap-types-subtype-validation-externref-to-anyref.wat\")))'"
(module
  (func (param externref) (result anyref)
    (ref.cast anyref (local.get 0))))
