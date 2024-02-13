//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function testCallIndirect() {
  {
    const m = instantiate(`
      (module
        (type (sub (func (param i32) (result i32))))
        (type (sub 0 (func (param i32) (result i32))))
        (table 10 (ref 1) (ref.func 1))
        (func (export "main") (result i32)
          ;; type index equality check, but with sub
          (call_indirect 0 (type 1) (i32.const 42) (i32.const 0)))
        (func (type 1) (i32.add (local.get 0) (i32.const 1)))
      )
    `);
    for (let i = 0; i < 10000; i++)
      assert.eq(m.exports.main(), 43);
  }

  {
    const m = instantiate(`
      (module
        (type (sub final (func (param i32) (result i32))))
        (table 10 funcref (ref.func 1))
        (func (export "main") (result i32)
          (call_indirect 0 (type 0) (i32.const 42) (i32.const 0)))
        (func (type 0) (i32.add (local.get 0) (i32.const 1)))
      )
    `);
    for (let i = 0; i < 10000; i++)
      assert.eq(m.exports.main(), 43);
  }

  {
    const m = instantiate(`
      (module
        (type (sub (func (param i32) (result i32))))
        (type (sub 0 (func (param i32) (result i32))))
        (table 10 (ref 1) (ref.func 1))
        (func (export "main") (result i32)
          (call_indirect 0 (type 0) (i32.const 42) (i32.const 0)))
        (func (type 1) (i32.add (local.get 0) (i32.const 1)))
      )
    `);
    for (let i = 0; i < 10000; i++)
      assert.eq(m.exports.main(), 43);
  }

  // Test deeper hierarchies
  {
    const m = instantiate(`
      (module
        (type (sub (func (param i32) (result))))
        (type (sub 0 (func (param i32) (result))))
        (type (sub 1 (func (param i32) (result))))
        (type (sub 2 (func (param i32) (result))))
        (type (sub 3 (func (param i32) (result))))
        (table 10 (ref 1) (ref.func 1))
        (func (export "main") (result)
          (call_indirect 0 (type 0) (i32.const 42) (i32.const 0))
          (call_indirect 0 (type 1) (i32.const 42) (i32.const 0))
          (call_indirect 0 (type 2) (i32.const 42) (i32.const 0))
          (call_indirect 0 (type 3) (i32.const 42) (i32.const 0))
          (call_indirect 0 (type 4) (i32.const 42) (i32.const 0)))
        (func (type 4) (i32.add (local.get 0) (i32.const 1)) drop)
      )
    `);
    for (let i = 0; i < 10000; i++)
      m.exports.main();
  }

  // Test failure cases
  {
    const m = instantiate(`
      (module
        (type (sub (func (param i32) (result))))
        (type (sub 0 (func (param i32) (result))))
        (type (sub 1 (func (param i32) (result))))
        (type (sub 2 (func (param i32) (result))))
        (type (sub 3 (func (param i32) (result))))
        (table 10 (ref 2) (ref.func 2))
        (func (export "type3") (result)
          (call_indirect 0 (type 3) (i32.const 42) (i32.const 0)))
        (func (export "type4") (result)
          (call_indirect 0 (type 4) (i32.const 42) (i32.const 0)))
        (func (type 2) (i32.add (local.get 0) (i32.const 1)) drop)
      )
    `);
    for (let i = 0; i < 10000; i++) {
      assert.throws(
          () => { m.exports.type3() },
          WebAssembly.RuntimeError,
          "call_indirect to a signature that does not match"
        );
      assert.throws(
          () => m.exports.type4(),
          WebAssembly.RuntimeError,
          "call_indirect to a signature that does not match"
        );
    }
  }

  // Final type that isn't a subtype of non-final type.
  {
    const m = instantiate(`
      (module
        (type (sub final (func (param i32) (result))))
        (type (sub (func (param i32) (result))))
        (table 10 (ref 0) (ref.func 1))
        (func (export "main") (result)
          (call_indirect 0 (type 1) (i32.const 42) (i32.const 0)))
        (func (type 0) (i32.add (local.get 0) (i32.const 1)) drop)
      )
    `);
     assert.throws(
       () => { m.exports.main() },
       WebAssembly.RuntimeError,
       "call_indirect to a signature that does not match"
     );
  }

  // Final type that *is* a subtype, so it's ok.
  {
    const m = instantiate(`
      (module
        (type (sub (func (param i32) (result))))
        (type (sub final 0 (func (param i32) (result))))
        (table 10 (ref 1) (ref.func 1))
        (func (export "main") (result)
          (call_indirect 0 (type 0) (i32.const 42) (i32.const 0)))
        (func (type 1) (i32.add (local.get 0) (i32.const 1)) drop)
      )
    `);
    for (let i = 0; i < 10000; i++)
      m.exports.main();
  }

  // Non-final type isn't a subtype of final type.
  {
    const m = instantiate(`
      (module
        (type (sub (func (param i32) (result))))
        (type (sub final (func (param i32) (result))))
        (table 10 (ref 0) (ref.func 1))
        (func (export "main") (result)
          (call_indirect 0 (type 1) (i32.const 42) (i32.const 0)))
        (func (type 0) (i32.add (local.get 0) (i32.const 1)) drop)
      )
    `);
     assert.throws(
       () => { m.exports.main() },
       WebAssembly.RuntimeError,
       "call_indirect to a signature that does not match"
     );
  }
}

testCallIndirect();
