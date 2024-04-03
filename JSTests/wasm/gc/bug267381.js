//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true", "--useWebAssemblyExtendedConstantExpressions=true")

import * as assert from "../assert.js";
import { instantiate } from "./wast-wrapper.js";

{
  const m1 = instantiate(`
    (module
      (type (struct (field i32)))
      (type (array (mut (ref null 0))))
      (func (export "maker") (result (ref 1))
        (array.new_default 1 (i32.const 5))))
  `);

  const arr = m1.exports.maker();
  assert.isObject(arr);

  // Do a GC to ensure the array is an old object.
  gc();

  const m2 = instantiate(`
    (module
      (type (struct (field i32)))
      (type (array (mut (ref null 0))))
      (func (export "set") (param (ref 1) i32)
        (array.set 1 (local.get 0) (local.get 1) (struct.new 0 (i32.const 42))))
      (func (export "get") (param (ref 1) i32) (result i32)
        (struct.get 0 0 (array.get 1 (local.get 0) (local.get 1)))))
  `);

  for (var i = 0; i < 5; i++)
    m2.exports.set(arr, i);

  // Do an eden GC to test write barriers.
  edenGC();

  for (var i = 0; i < 5; i++)
    assert.eq(m2.exports.get(arr, i), 42);
}

// Test array.new_fixed case.
{
  const m = instantiate(`
    (module
      (type (struct (field i32)))
      (type (array (mut (ref null 0))))
      (func (export "maker") (result (ref 1))
        (array.new_fixed 1 2 (struct.new 0 (i32.const 42)) (struct.new 0 (i32.const 42))))
      (func (export "get") (param (ref 1) i32) (result i32)
        (struct.get 0 0 (array.get 1 (local.get 0) (local.get 1)))))
  `);

  const arr = m.exports.maker();
  gc();
  assert.eq(m.exports.get(arr, 0), 42);
  assert.eq(m.exports.get(arr, 1), 42);
}
