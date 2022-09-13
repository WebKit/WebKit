//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

function testArrayDeclaration() {
  instantiate(`
    (module
      (type (array i32))
    )
  `);

  instantiate(`
    (module
      (type (array (mut i32)))
    )
  `);

  /*
   * invalid element type
   * (module
   *   (type (array <invalid>))
   * )
   */
  assert.throws(
    () => new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x5e\xff\x02")),
    WebAssembly.CompileError,
    "Module doesn't parse at byte 17: can't get array's element Type"
  )

  /*
   * invalid mut value 0x02
   * (module
   *   (type (array (<invalid> i32)))
   * )
   */
  assert.throws(
    () => new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x5e\x7f\x02")),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 18: invalid array mutability: 0x02"
  )

  instantiate(`
    (module
      (type (array i32))
      (func (result (ref null 0)) (ref.null 0))
    )
  `);

  instantiate(`
    (module
      (type (array i32))
      (func (result arrayref) (ref.null 0))
    )
  `);

  assert.throws(
    () => compile(`
      (module
        (type (array i32))
        (func (result (ref null 0)) (ref.null array))
      )
    `),
    WebAssembly.CompileError,
    "control flow returns with unexpected type. RefNull is not a RefNull, in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
        (type (array i32))
        (func (result funcref) (ref.null 0))
      )
    `),
    WebAssembly.CompileError,
    "control flow returns with unexpected type. RefNull is not a RefNull, in function at index 0"
  );
}

function testArrayJS() {
  // JS API behavior not specified yet, import/export error for now.
  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (type (array i64))
          (func (export "f") (result (ref null 0))
            (ref.null 0))
        )
      `);
      m.exports.f();
    },
    WebAssembly.RuntimeError,
    "Unsupported use of struct or array type"
  )

  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (type (array externref))
          (func (export "f") (param (ref null 0)))
        )
      `);
      m.exports.f(null);
    },
    WebAssembly.RuntimeError,
    "Unsupported use of struct or array type"
  )

  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (type (array f32))
          (import "m" "f" (func (param (ref null 0))))
          (func (export "g") (call 0 (ref.null 0)))
        )
      `, { m: { f: (x) => { return; } } });
      m.exports.g();
    },
    WebAssembly.RuntimeError,
    "Unsupported use of struct or array type"
  )

  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (type (array i32))
          (import "m" "f" (func (result (ref null 0))))
          (func (export "g") (call 0) drop)
        )
      `, { m: { f: (x) => { return null; } } });
      m.exports.g();
    },
    WebAssembly.RuntimeError,
    "Unsupported use of struct or array type"
  )

  // JS API behavior not specified yet, setting global errors for now.
  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (type (array i32))
          (global (export "g") (mut (ref null 0)) (ref.null 0))
        )
      `);
      m.exports.g.value = 42;
    },
    WebAssembly.RuntimeError,
    "Unsupported use of struct or array type"
  )
}

testArrayDeclaration();
testArrayJS();
