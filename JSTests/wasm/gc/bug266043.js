//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

const m1 = instantiate(`
  (module
    (type (struct (field i32)))
    (type (struct (field (mut (ref null 0)))))
    (func (export "f") (result (ref any))
      (struct.new 1 (ref.null 0)))
  )
`);

// GC to ensure struct is an old object.
const struct = m1.exports.f()
gc();

const m2 = instantiate(`
  (module
    (type (struct (field i32)))
    (type (struct (field (mut (ref null 0)))))
    (func (export "g") (param (ref 1))
      (struct.set 1 0 (local.get 0) (struct.new 0 (i32.const 42))))
    (func (export "h") (param (ref 1)) (result i32)
      (struct.get 0 0 (struct.get 1 0 (local.get 0))))
  )
`);

// Do an eden GC for the new struct allocated in m2. Write barriers
// should ensure that the object will be kept live.
m2.exports.g(struct);
edenGC();
assert.eq(m2.exports.h(struct), 42);
