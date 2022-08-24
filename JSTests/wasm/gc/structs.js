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

function testStructDeclaration() {
  instantiate(`
    (module
      (type (struct))
    )
  `);

  instantiate(`
    (module
      (type (struct (field i32)))
    )
  `);

  instantiate(`
    (module
      (type (struct (field i32) (field i32)))
    )
  `);

  instantiate(`
    (module
      (type (struct (field (mut i32))))
    )
  `);

  instantiate(`
    (module
      (type $Point (struct (field (mut i32) (mut i32))))
      (type $Line (struct (field (mut (ref $Point)) (mut (ref $Point)))))
    )
  `);

  /*
   * too many fields
   * (module
   *   (type (struct (field i32)))
   * )
   */
  assert.throws(
    () =>
      module(
        "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x07\x01\x5f\xff\xff\xff\x7f\x00"
      ),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 16: number of fields for struct type at position 0 is too big 268435455 maximum 10000 (evaluating 'new WebAssembly.Module(buffer)')"
  );

  /*
   * invalid mutability
   * (module
   *   (type (struct (field (mut i32))))
   * )
   */
  assert.throws(
    () =>
      module(
        "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x05\x01\x5f\x01\x7f\x02"
      ),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 15: invalid Field's mutability: 0x02 (evaluating 'new WebAssembly.Module(buffer)')"
  );
}

function testStructJS() {
  // JS API behavior not specified yet, import/export error for now.
  assert.throws(
    () => {
      let m = instantiate(`
        (module
          (type (struct))
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
          (type (struct))
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
          (type (struct))
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
          (type (struct))
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
          (type (struct))
          (global (export "g") (mut (ref null 0)) (ref.null 0))
        )
      `);
      m.exports.g.value = 42;
    },
    WebAssembly.RuntimeError,
    "Unsupported use of struct or array type"
  )
}

testStructDeclaration();
testStructJS();
