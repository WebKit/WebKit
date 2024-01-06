//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true", "--useWebAssemblyExtendedConstantExpressions=true")

import { compile, instantiate } from "./wast-wrapper.js";

instantiate(`
 (module
   (type (sub (array (mut i16))))
   (global (mut (ref null 0)) (ref.null 0))
   (func (export "init")
     (global.set 0 (array.new 0 (i32.const 42) (i32.const 5)))
     (array.set 0 (global.get 0) (i32.const 3) (i32.and (i32.const 84) (i32.const 0xFFFF))))
   (func (export "get") (param i32) (result i32)
     (array.get_u 0 (global.get 0) (local.get 0)))
   )
`).exports.init();
