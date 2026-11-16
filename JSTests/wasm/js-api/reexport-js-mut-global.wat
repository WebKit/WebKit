(module
  (import "./js-module-mutable-global-export.js" "g" (global $g (mut i32)))
  (export "g" (global $g)))
