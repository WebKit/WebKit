;; Tools/Scripts/run-jsc -e "'load(\"JSTests/wasm/gc/wast.js\"); writeFile(\"JSTests/wasm/stress/type-index-abstract-heap-types-subtype-validation-structref-to-eqref.wasm\", WebAssemblyText.encode(read(\"JSTests/wasm/stress/type-index-abstract-heap-types-subtype-validation-structref-to-eqref.wat\")))'"
(module
  (func (param structref) (result eqref)
    (ref.cast eqref (local.get 0))))
