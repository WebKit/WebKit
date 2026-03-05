import * as assert from "../assert.js";
import { instantiate } from "./wast-wrapper.js";

function testI31refImport(value) {
  let jsFunc = () => value;

  let m = instantiate(`
    (module
      (import "env" "getNumber" (func $getNumber (result i31ref)))
      (func (export "test") (result i32)
        (call $getNumber)
        (i31.get_s))
    )
  `, { env: { getNumber: jsFunc } });

  let result = m.exports.test();
  assert.eq(result, value);
}

function testEqrefImport(value) {
  let jsFunc = () => value;

  let m = instantiate(`
    (module
      (import "env" "getNumber" (func $getNumber (result eqref)))
      (func (export "test") (result i32)
        (call $getNumber)
        (ref.cast (ref i31))
        (i31.get_s))
    )
  `, { env: { getNumber: jsFunc } });

  let result = m.exports.test();
  assert.eq(result, value);
}

function testAnyrefImport(value) {
  let jsFunc = () => value;

  let m = instantiate(`
    (module
      (import "env" "getNumber" (func $getNumber (result anyref)))
      (func (export "test") (result i32)
        (call $getNumber)
        (ref.cast (ref i31))
        (i31.get_s))
    )
  `, { env: { getNumber: jsFunc } });

  let result = m.exports.test();
  assert.eq(result, value);
}

testI31refImport(42);
testI31refImport(42.0);
testI31refImport(-3251.0);
// Max i31ref value: 2^30 - 1 = 1073741823
testI31refImport(1073741823);
testI31refImport(1073741823.0);
// Min i31ref value: -2^30 = -1073741824
testI31refImport(-1073741824);
testI31refImport(-1073741824.0);

assert.throws(() => testI31refImport(42.5),
    TypeError,
    "Argument value did not match the reference type");
assert.throws(() => testI31refImport(1073741824),
    TypeError,
    "Argument value did not match the reference type");
assert.throws(() => testI31refImport(-1073741825),
    TypeError,
    "Argument value did not match the reference type");


testEqrefImport(42);
testEqrefImport(42.0);
testEqrefImport(-3251.0);
// Max i31ref value: 2^30 - 1 = 1073741823
testEqrefImport(1073741823);
testEqrefImport(1073741823.0);
// Min i31ref value: -2^30 = -1073741824
testEqrefImport(-1073741824);
testEqrefImport(-1073741824.0);

assert.throws(() => testEqrefImport(42.5),
    TypeError,
    "Argument value did not match the reference type");
assert.throws(() => testEqrefImport(1073741824),
    TypeError,
    "Argument value did not match the reference type");
assert.throws(() => testEqrefImport(-1073741825),
    TypeError,
    "Argument value did not match the reference type");


testAnyrefImport(42);
testAnyrefImport(42.0);
testAnyrefImport(-3251.0);
// Max i31ref value: 2^30 - 1 = 1073741823
testAnyrefImport(1073741823);
testAnyrefImport(1073741823.0);
// Min i31ref value: -2^30 = -1073741824
testAnyrefImport(-1073741824);
testAnyrefImport(-1073741824.0);

assert.throws(() => testAnyrefImport(42.5),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type");
assert.throws(() => testAnyrefImport(1073741824),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type");
assert.throws(() => testAnyrefImport(-1073741825),
    WebAssembly.RuntimeError,
    "ref.cast failed to cast reference to target heap type");

