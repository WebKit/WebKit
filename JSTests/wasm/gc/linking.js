//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function testLinking() {
  {
    const m0 = instantiate(`
      (module
        (type (sub (func)))
        (type (sub 0 (func)))
        (func (export "f0") (type 1)))
    `);

    const m1 = instantiate(`
      (module
        (type (sub (func)))
        (func (import "m" "f1") (type 0)))
    `, { m: { f1: m0.exports.f0 } });
  }

  {
    const m0 = instantiate(`
      (module
        (type (func))
        (table (export "t0") 10 (ref null 0)))
    `);

    const m1 = instantiate(`
      (module
        (table (import "m" "t1") 10 (ref null func)))
    `, { m: { t1: m0.exports.t0 } });
  }

  {
    const m0 = instantiate(`
      (module
        (type (func))
        (global (export "g0") (ref null 0) (ref.null 0)))
    `);

    const m1 = instantiate(`
      (module
        (global (import "m" "g1") (ref null func)))
    `, { m: { g1: m0.exports.g0 } });
  }

  {
    const m0 = instantiate(`
      (module
        (type (func))
        (global (export "g0") (mut (ref null 0)) (ref.null 0)))
    `);

    const m1 = instantiate(`
      (module
        (global (import "m" "g1") (mut (ref null func))))
    `, { m: { g1: m0.exports.g0 } });
  }
}

testLinking();
