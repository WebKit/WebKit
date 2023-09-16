//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function runWasmGCObjectTests(obj) {
  assert.eq(obj.foo, undefined);
  assert.eq(obj["foo"], undefined);
  assert.eq(obj[0], undefined);
  assert.eq(Object.getOwnPropertyNames(obj).length, 0);
  assert.eq(Object.hasOwn(obj, "foo"), false);
  assert.eq(Object.prototype.hasOwnProperty.apply(obj, ["foo"]), false);
  assert.eq("foo" in obj, false);
  assert.eq(Object.getOwnPropertyDescriptor(obj, "foo"), undefined);
  assert.throws(
    () => { obj.foo = 42; },
    TypeError,
    "Cannot set property for WebAssembly GC object"
  );
  assert.throws(
    () => { obj[0] = 5; },
    TypeError,
    "Cannot set property for WebAssembly GC object"
  );
  assert.throws(
    () => { delete obj.foo; },
    TypeError,
    "Cannot delete property for WebAssembly GC object"
  );
  assert.throws(
    () => { delete obj[0]; },
    TypeError,
    "Cannot delete property for WebAssembly GC object"
  );
  assert.throws(
    () => Object.defineProperty(obj, "foo", { value: 42 }),
    TypeError,
    "Cannot define property for WebAssembly GC object"
  )
  assert.eq(Object.getPrototypeOf(obj), null);
  assert.throws(
    () => Object.setPrototypeOf(obj, {}),
    TypeError,
    "Cannot set prototype of WebAssembly GC object"
  )
  assert.eq(Object.isExtensible(obj), false);
  assert.throws(
    () => Object.preventExtensions(obj),
    TypeError,
    "Cannot run preventExtensions operation on WebAssembly GC object"
  )
  assert.throws(
    () => Object.seal(obj),
    TypeError,
    "Cannot run preventExtensions operation on WebAssembly GC object"
  )
}

function testArray() {
  let m = instantiate(`
    (module
      (type (array i32))
      (func (export "make") (result (ref 0))
        (array.new 0 (i32.const 42) (i32.const 5)))
    )
  `);
  const arr = m.exports.make();
  runWasmGCObjectTests(arr, m.exports.make);
}

function testStruct() {
  let m = instantiate(`
    (module
      (type (struct (field i32)))
      (func (export "make") (result (ref 0))
        (struct.new 0 (i32.const 42)))
    )
  `);
  const struct = m.exports.make();
  runWasmGCObjectTests(struct, m.exports.make);
}

testArray();
testStruct();
