;; Tools/Scripts/run-jsc -e "'load(\"JSTests/wasm/gc/wast.js\"); writeFile(\"JSTests/wasm/stress/type-index-abstract-heap-types-subtype-validation-i31ref-to-anyref.wasm\", WebAssemblyText.encode(read(\"JSTests/wasm/stress/type-index-abstract-heap-types-subtype-validation-i31ref-to-anyref.wat\")))'"
(module
  (func (param i31ref) (result anyref)
    (ref.cast anyref (local.get 0))))
