//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import { instantiate } from "./wast-wrapper.js";

instantiate(`
  (module
    (type (struct (field (mut i32))))
    (func (result i32)
      (local $19 (ref 0))
      (local $22 i32)
      (local.set $19 (struct.new_default 0))
      (i32.const 1)
      if (result i32)
        (local.get $19)
        (block (result i32)
          (i32.const 1)
          (br 0))
        (struct.set 0 0)
        (i32.const 1)
      else
        (i32.const 0)
      end))
`);
