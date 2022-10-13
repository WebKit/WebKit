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

function testRecDeclaration() {
  /*
   * This test needs to be in binary format, as it tests the specific encoding
   * of the recursion group. This one omits the explicit `rec`.
   *
   *  (module
   *    (rec (type (array (ref 0))))
   *    (func (result (ref null 0)) (ref.null 0)))
   */
  new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x5e\x6b\x00\x00\x60\x00\x01\x6c\x00\x03\x82\x80\x80\x80\x00\x01\x01\x0a\x8a\x80\x80\x80\x00\x01\x84\x80\x80\x80\x00\x00\xd0\x00\x0b"))

  // Same test as above but using a non-shorthand encoding.
  new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x4f\x01\x5e\x6b\x00\x00\x60\x00\x01\x6c\x00\x03\x82\x80\x80\x80\x00\x01\x01\x0a\x8a\x80\x80\x80\x00\x01\x84\x80\x80\x80\x00\x00\xd0\x00\x0b"))

  // Test invalid reference type index in a recursion group.
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x5e\x6b\x01\x00\x60\x00\x01\x6c\x00\x03\x82\x80\x80\x80\x00\x01\x01\x0a\x8a\x80\x80\x80\x00\x01\x84\x80\x80\x80\x00\x00\xd0\x00\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 18: can't get array's element Type"
  );

  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x4f\x01\x5e\x6b\x01\x00\x60\x00\x01\x6c\x00\x03\x82\x80\x80\x80\x00\x01\x01\x0a\x8a\x80\x80\x80\x00\x01\x84\x80\x80\x80\x00\x00\xd0\x00\x0b"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 20: can't get array's element Type"
  );

  /*
   *  Test invalid index in a recursion group with more than one type.
   *
   *  (module (rec (type (array (ref 2))) (type (array (ref 1)))))
   */
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8b\x80\x80\x80\x00\x01\x4f\x02\x5e\x6b\x02\x00\x5e\x6b\x01\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 20: can't get array's element Type"
  );

  instantiate(`
     (module
       (type (struct (field (ref 0))))
       (func (result (ref null 0)) (ref.null 0)))
  `);

  instantiate(`
     (module
       (type (array (ref 0)))
       (func (result (ref null array)) (ref.null 0)))
  `);

  instantiate(`
     (module
       (type (func (param (ref 0))))
       (func (result (ref null func)) (ref.null 0)))
  `);

  instantiate(`
    (module
      (rec (type (func)) (type (struct)))
    )
  `);

  instantiate(`
    (module
      (rec (type (func)) (type (array i32)))
    )
  `);

  instantiate(`
    (module
      (rec (type (func)) (type (struct)))
      (func (type 0))
    )
  `);

  instantiate(`
    (module
      (rec (type (func)) (type (array f32)))
      (func (type 0))
    )
  `);

  assert.throws(
    () => compile(`
      (module
        (rec (type (struct)) (type (func)))
        (func (type 0))
      )
    `),
    WebAssembly.CompileError,
    "type signature was not a function signature"
  );

  assert.throws(
    () => compile(`
      (module
        (rec (type (array i64)) (type (func)))
        (func (type 0))
      )
    `),
    WebAssembly.CompileError,
    "type signature was not a function signature"
  );

  instantiate(`
    (module
      (rec
        (type (func (result (ref 1))))
        (type (func (result (ref 0)))))
    )
  `);

  instantiate(`
    (module
      (rec
        (type (func))
        (type (func (result (ref 0)))))
    )
  `);

  instantiate(`
    (module
      (rec (type (func)) (type (struct)))
      (rec (type (func)) (type (struct)))
      (elem declare funcref (ref.func 0))
      (func (type 0))
      (func (result (ref 2)) (ref.func 0))
    )
  `);

  {
    let m1 = instantiate(`
      (module
        (rec (type (func)) (type (struct)))
        (func (export "f") (type 0))
      )
    `);
    instantiate(`
      (module
        (rec (type (func)) (type (struct)))
        (func (import "m" "f") (type 0))
        (start 0)
      )
    `, { m: { f: m1.exports.f } });
  }

  {
    let m1 = instantiate(`
      (module
        (rec (type (func)) (type (struct)))
        (func (export "f") (type 0))
      )
    `);
    assert.throws(
      () => instantiate(`
        (module
          (rec (type (struct)) (type (func)))
          (func (import "m" "f") (type 1))
          (start 0)
        )
      `, { m: { f: m1.exports.f } }),
      WebAssembly.LinkError,
      "imported function m:f signature doesn't match the provided WebAssembly function's signature"
    );
  }

  {
    let m1 = instantiate(`
      (module
        (rec (type (func)) (type (struct)))
        (elem declare funcref (ref.func 0))
        (func)
        (func (export "f") (result (ref 0)) (ref.func 0))
      )
    `);
    assert.throws(
      () => instantiate(`
        (module
          (rec (type (struct)) (type (func)))
          (func (import "m" "f") (type 1))
        )
      `, { m: { f: m1.exports.f } }),
      WebAssembly.LinkError,
      "imported function m:f signature doesn't match the provided WebAssembly function's signature"
    );
  }

  assert.throws(
    () => compile(`
      (module
        (rec (type (func)) (type (struct)))
        (rec (type (struct)) (type (func)))
        (global (ref 0) (ref.func 0))
        (func (type 3))
      )
    `),
    WebAssembly.CompileError,
    "Global init_expr opcode of type Ref doesn't match global's type Ref"
  );

  instantiate(`
    (module
      (rec (type (func (result (ref 1))))
           (type (func (result (ref 0)))))
      (elem declare funcref (ref.func 0))
      (elem declare funcref (ref.func 1))
      (func (type 0) (ref.func 1))
      (func (type 1) (ref.func 0))
    )
  `);

  assert.throws(
    () => compile(`
      (module
        (rec (type (func (result (ref 1))))
             (type (func (result (ref 0)))))
        (elem declare funcref (ref.func 0))
        (elem declare funcref (ref.func 1))
        (func (type 0) (ref.func 1))
        (func (type 1) (ref.func 1))
      )
    `),
    WebAssembly.CompileError,
    "control flow returns with unexpected type. Ref is not a Ref, in function at index 1"
  );

  instantiate(`
    (module
      (rec (type (func (param i32))) (type (struct)))
      (elem declare funcref (ref.func 0))
      (func (type 0))
      (func (call_ref (i32.const 42) (ref.func 0)))
      (start 1)
    )
  `);

  instantiate(`
    (module
      (rec (type (func (result i32))) (type (struct)))
      (rec (type (struct)) (type (func (result i32))))
      (func (type 0)
        (block (type 3)
          (i32.const 42)))
    )
  `);

  instantiate(`
    (module
      (rec (type (func (result i32))) (type (struct)))
      (rec (type (struct)) (type (func (result i32))))
      (func (type 0)
        (loop (type 3)
          (i32.const 42)))
    )
  `);

  instantiate(`
    (module
      (rec (type (func (result i32))) (type (struct)))
      (rec (type (struct)) (type (func (result i32))))
      (func (type 0)
        (i32.const 1)
        (if (type 3) (then (i32.const 42)) (else (i32.const 43))))
    )
  `);

  instantiate(`
    (module
      (rec (type (func)) (type (struct)))
      (table 5 funcref)
      (elem (offset (i32.const 0)) funcref (ref.func 0))
      (func (type 0))
      (func (call_indirect (type 0) (i32.const 0)))
      (start 1)
    )
  `);

  // Ensure implicit rec groups are accounted for, and treated
  // correctly with regards to equality.
  instantiate(`
    (module
      (type $a (struct (field i32)))
      (rec (type $b (struct (field i32))))
      (type $c (struct (field i32)))

      (func (result (ref null $a)) (ref.null $b))
      (func (result (ref null $a)) (ref.null $c))
      (func (result (ref null $b)) (ref.null $a))
      (func (result (ref null $b)) (ref.null $c))
      (func (result (ref null $c)) (ref.null $a))
      (func (result (ref null $c)) (ref.null $b)))
  `);

  // This is the same test as above, but using a particular binary encoding.
  // The encoding for this test specifically uses both shorthand and the full
  // rec form to test the equivalence of the two.
  new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x9e\x80\x80\x80\x00\x06\x5f\x01\x7f\x00\x4f\x01\x5f\x01\x7f\x00\x5f\x01\x7f\x00\x60\x00\x01\x6c\x00\x60\x00\x01\x6c\x01\x60\x00\x01\x6c\x02\x03\x87\x80\x80\x80\x00\x06\x03\x03\x04\x04\x05\x05\x0a\xb7\x80\x80\x80\x00\x06\x84\x80\x80\x80\x00\x00\xd0\x01\x0b\x84\x80\x80\x80\x00\x00\xd0\x02\x0b\x84\x80\x80\x80\x00\x00\xd0\x00\x0b\x84\x80\x80\x80\x00\x00\xd0\x02\x0b\x84\x80\x80\x80\x00\x00\xd0\x00\x0b\x84\x80\x80\x80\x00\x00\xd0\x01\x0b"));
}

testRecDeclaration();
