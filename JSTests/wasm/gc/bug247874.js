//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true", "--collectContinuously=true")

import { compile, instantiate } from "./wast-wrapper.js";

// Test that with compound type definitions like recursion and subtypes that
// type definitions don't get de-allocated too soon.
//
// Even with continuous collection enabled, the test is somewhat non-deterministic
// so we loop several times to ensure it triggers.
for (let i = 0; i < 10; i++) {
  instantiate(`(module (type (func)))`);
  let m1 = instantiate(`
    (module
      (type (struct))
      (type (sub 0 (struct (field i32))))
      (global (export "g") (ref null 1) (ref.null 1))
    )
  `);
  instantiate(`
    (module
      (type (struct))
      (type (sub 0 (struct (field i32))))
      (global (import "m" "g") (ref null 1))
    )
  `, { m: { g: m1.exports.g } });
}

for (let i = 0; i < 10; i++) {
  instantiate(`(module (type (func)))`);
  let m1 = instantiate(`
    (module
      (rec (type (struct)) (type (func)))
      (global (export "g") (ref null 1) (ref.null 1))
    )
  `);
  instantiate(`
    (module
      (rec (type (struct)) (type (func)))
      (global (import "m" "g") (ref null 1))
    )
  `, { m: { g: m1.exports.g } });
}
