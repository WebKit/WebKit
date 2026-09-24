import * as assert from "../assert.js";
import { compile, instantiate, watToWasm } from "./wast-wrapper.js";

function testValidation() {
  assert.compileError(watToWasm(`
      (module
        (func (result i32)
          (ref.eq (ref.null any) (ref.null array))))
    `),
    "WebAssembly.Module doesn't validate: ref.eq ref1 to type RefNull expected Eqref, in function at index 0");

  assert.compileError(watToWasm(`
      (module
        (func (param anyref) (result))
        (func (call 0 (ref.null extern))))
    `),
    "WebAssembly.Module doesn't validate: argument type mismatch in call, got (ref null extern), expected (ref null any), in function at index 1");

  assert.compileError(watToWasm(`
      (module
        (type (struct))
        (func (param (ref null 0)) (result))
        (func (call 0 (ref.null any))))
    `),
    "WebAssembly.Module doesn't validate: argument type mismatch in call, got (ref null any), expected (ref null <struct:0>), in function at index 1");

  assert.compileError(watToWasm(`
      (module
        (func (param (ref null none)) (result))
        (func (call 0 (ref.null any))))
    `),
    "WebAssembly.Module doesn't validate: argument type mismatch in call, got (ref null any), expected (ref null none), in function at index 1");

  assert.compileError(watToWasm(`
      (module
        (type (struct))
        (func (param (ref none)) (result))
        (func (call 0 (struct.new 0))))
    `),
    "WebAssembly.Module doesn't validate: argument type mismatch in call, got (ref <struct:0>), expected (ref none), in function at index 1");
}

function testAnyref() {
  instantiate(`
    (module
      (func (param anyref) (result))
      (func (export "f") (call 0 (ref.null array))))
  `).exports.f();

  instantiate(`
    (module
      (type (struct))
      (func (param anyref) (result))
      (func (export "f") (call 0 (struct.new 0))))
  `).exports.f();
}

function testNullref() {
  instantiate(`
    (module
      (func (param nullref) (result))
      (func (export "f") (call 0 (ref.null none))))
  `).exports.f();

  instantiate(`
    (module
      (func (result nullref) (local nullref)
        (local.set 0 (ref.null none))
        (local.get 0))
      (func (export "f") (call 0) (drop)))
  `).exports.f();
}

function testNullfuncref() {
  instantiate(`
    (module
      (func (param nullfuncref) (result))
      (func (export "f") (call 0 (ref.null nofunc))))
  `).exports.f();

  instantiate(`
      (module
        (func (export "f") (result funcref) (ref.null nofunc)))
  `).exports.f();

  assert.compileError(watToWasm(`
      (module
        (func (export "f") (result nullfuncref) (ref.null func)))
    `),
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. (ref null func) is not a (ref null nofunc)")

  assert.compileError(watToWasm(`
      (module
        (func (export "f") (result nullref) (ref.null nofunc)))
    `),
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. (ref null nofunc) is not a (ref null none)")

  instantiate(`
    (module
      (func (result nullfuncref) (local nullfuncref)
        (local.set 0 (ref.null nofunc))
        (local.get 0))
      (func (export "f") (call 0) (drop)))
  `).exports.f();
}

function testNullexternref() {
  instantiate(`
    (module
      (func (param nullexternref) (result))
      (func (export "f") (call 0 (ref.null noextern))))
  `).exports.f();

  instantiate(`
      (module
        (func (export "f") (result externref) (ref.null noextern)))
  `).exports.f();

  assert.compileError(watToWasm(`
      (module
        (func (export "f") (result nullexternref) (ref.null extern)))
    `),
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. (ref null extern) is not a (ref null noextern)")

  assert.compileError(watToWasm(`
      (module
        (func (export "f") (result nullref) (ref.null noextern)))
    `),
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. (ref null noextern) is not a (ref null none)")

  instantiate(`
    (module
      (func (result nullexternref) (local nullexternref)
        (local.set 0 (ref.null noextern))
        (local.get 0))
      (func (export "f") (call 0) (drop)))
  `).exports.f();
}

testValidation();
testAnyref();
testNullref();
testNullfuncref();
testNullexternref();
