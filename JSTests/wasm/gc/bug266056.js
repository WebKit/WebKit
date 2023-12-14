//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

{
  let m = instantiate(`
    (module
      (type (struct))
      (func (export "f") (param i32) (result (ref null struct))
        (select (result (ref null struct)) (struct.new 0) (ref.null struct) (local.get 0))
       )
      )
  `);
  assert.isObject(m.exports.f(1));
  assert.eq(m.exports.f(0), null);
}
