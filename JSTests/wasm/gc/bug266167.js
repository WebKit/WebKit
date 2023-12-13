//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

instantiate(`
  (module
    (type (struct (field i32)))
    (func (export "f")
      (block
        (struct.new 0 (i32.const 42))
        (br_on_null 0)
        (struct.get 0 0)
        drop
        ))
  )
`).exports.f();

{
  let m = instantiate(`
    (module
      (type (struct (field i32)))
      (func (export "f") (result i32)
        (block (result i32)
          (i32.const -1)
          (ref.i31 (i32.const 42))
          (br_on_null 0)
          (br 0 (i31.get_u))))
    )
  `);
  assert.eq(m.exports.f(), 42);
}
