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

function testSubDeclaration() {
  instantiate(`
    (module
      (type (sub (struct)))
      (type (sub 0 (struct)))
    )
  `);

  instantiate(`
    (module
      (type (sub (func)))
      (type (sub 0 (func)))
    )
  `);

  instantiate(`
    (module
      (type (sub (array i32)))
      (type (sub 0 (array i32)))
    )
  `);

  instantiate(`
    (module
      (type (sub (array (mut i32))))
      (type (sub 0 (array (mut i32))))
    )
  `);

  instantiate(`
    (module
      (type (sub (func)))
      (type (sub 0 (func)))
      (func (type 1))
    )
  `);

  instantiate(`
    (module
      (type (sub (func)))
      (type (sub (func (result funcref))))
      (type (sub 1 (func (result (ref 0)))))
    )
  `);

  instantiate(`
    (module
      (type (sub (func)))
      (type (sub (func (param (ref 0)))))
      (type (sub 1 (func (param funcref))))
    )
  `);

  instantiate(`
    (module
      (type (sub (struct)))
      (type (sub 0 (struct (field i32))))
    )
  `);

  instantiate(`
    (module
      (type (func))
      (type (sub (array (ref func))))
      (type (sub 1 (array (ref 0))))
    )
  `);

  instantiate(`
    (module
      (rec
        (type $r1 (sub (struct (field i32 (ref $r1))))))
      (rec
        (type $r2 (sub $r1 (struct (field i32 (ref $r3)))))
        (type $r3 (sub $r1 (struct (field i32 (ref $r2))))))
    )
  `);

  assert.throws(
    () => compile(`
      (module
        (type (func))
        (type (sub 0 (struct)))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 23: cannot declare subtype of final supertype"
  );

  assert.throws(
    () => compile(`
      (module
        (type (func))
        (type (sub 0 (array i32)))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 24: cannot declare subtype of final supertype"
  );

  /*
   * (module
   *   (type (func))
   *   ;; multiple supertypes not supported in MVP, not possible to express in text format.
   *   (type (sub (0 0) (struct)))
   * )
   */
  assert.throws(
    () => new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x50\x02\x00\x00\x5f\x00")),
    WebAssembly.CompileError,
    "number of supertypes for subtype at position 1 is too big"
  );

  assert.throws(
    () => compile(`
      (module
        (type (sub (struct (field i32))))
        (type (sub 0 (struct)))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  assert.throws(
    () => compile(`
      (module
        (type (sub (struct (field (mut i32)))))
        (type (sub 0 (struct (field i32))))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  assert.throws(
    () => compile(`
      (module
        (type (sub (struct (field i32))))
        (type (sub 0 (struct (field (mut i32)))))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  assert.throws(
    () => compile(`
      (module
        (type (sub (struct (field i64))))
        (type (sub 0 (struct (field i32))))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  instantiate(`
    (module
      (type (func))
      (type (sub (struct (field funcref))))
      (type (sub 1 (struct (field (ref 0)))))
    )
  `);

  assert.throws(
    () => compile(`
      (module
        (type (func))
        (type (sub (struct (field (mut funcref)))))
        (type (sub 1 (struct (field (mut (ref 0))))))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  instantiate(`
    (module
      (type (sub (struct)))
      (type (sub 0 (struct (field i32))))
      (func (result (ref null 0)) (ref.null 1))
    )
  `);

  instantiate(`
    (module
      (type (sub (struct)))
      (type (sub 0 (struct (field i32))))
      (type (sub 1 (struct (field i32) (field i64))))
      (func (result (ref null 0)) (ref.null 2))
      (func (result (ref null 1)) (ref.null 2))
    )
  `);

  instantiate(`
    (module
      (type (sub (struct)))
      (type (sub 0 (struct (field i32))))
      (type (sub 1 (struct (field i32) (field i64))))
      (type (sub 2 (struct (field i32) (field i64) (field f32))))
      (func (result (ref null 0)) (ref.null 3))
      (func (result (ref null 1)) (ref.null 3))
      (func (result (ref null 2)) (ref.null 3))
    )
  `);

  assert.throws(
    () => compile(`
      (module
        (type (sub (struct)))
        (type (sub 0 (struct (field i32))))
        (type (sub 1 (struct (field i32) (field i64))))
        (type (sub 1 (struct (field i32) (field f64))))
        (type (sub 2 (struct (field i32) (field i64) (field f32))))
        (func (result (ref null 3)) (ref.null 4))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. ((((())(I32, mutable))(I32, mutable, I64, mutable))(I32, mutable, I64, mutable, F32, mutable)) is not a (((())(I32, mutable))(I32, mutable, F64, mutable)), in function at index 0"
  );

  assert.throws(
    () => compile(`
      (module
        (type $s (sub (func)))
        (type $t (sub $s (func)))
        (elem declare funcref (ref.func 0))
        (func (type $s))
        (func (result (ref $t)) (ref.func 0))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. (() -> []) is not a ((() -> [])() -> []), in function at index 1 (evaluating 'new WebAssembly.Module(binary)')"
  );

  instantiate(`
    (module
      (rec
        (type (sub (struct)))
        (type (sub 0 (struct (field i32)))))
    )
  `);

  assert.throws(
    () => compile(`
      (module
        (rec
          (type (sub (struct (field f32))))
          (type (sub 0 (struct (field i32)))))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  assert.throws(
    () => compile(`
      (module
        (type (sub (func)))
        (type (sub 0 (func (result i32))))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  assert.throws(
    () => compile(`
      (module
        (type (sub (func)))
        (type (sub 0 (func (param i32))))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  assert.throws(
    () => compile(`
      (module
        (type (sub (func (param i32))))
        (type (sub 0 (func)))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  assert.throws(
    () => compile(`
      (module
        (type (func))
        (type (sub (func (param funcref))))
        (type (sub 1 (func (param (ref 0)))))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  assert.throws(
    () => compile(`
      (module
        (type (func))
        (type (sub (func (result (ref 0)))))
        (type (sub 1 (func (result funcref))))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  assert.throws(
    () => compile(`
      (module
        (type (sub (array (mut i32))))
        (type (sub 0 (array i32)))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  assert.throws(
    () => compile(`
      (module
        (type (sub (array i32)))
        (type (sub 0 (array (mut i32))))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  assert.throws(
    () => compile(`
      (module
        (type (func))
        (type (sub (array (mut (ref func)))))
        (type (sub 1 (array (mut (ref 0)))))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  {
    let m1 = instantiate(`
      (module
        (type (func))
        (type (sub (struct)))
        (type (sub 1 (struct (field i32))))
        (global (export "g") (ref null 2) (ref.null 2))
      )
    `);
    instantiate(`
      (module
        (type (sub (struct)))
        (type (sub 0 (struct (field i32))))
        (global (import "m" "g") (ref null 1))
      )
    `, { m: { g: m1.exports.g } });
  }

  {
    let m1 = instantiate(`
      (module
        (type (func))
        (type (sub (struct)))
        (type (sub 1 (struct (field i32))))
        (global (export "g") (ref null 2) (ref.null 2))
      )
    `);
    instantiate(`
      (module
        (type (sub (struct)))
        (type (sub 0 (struct (field i32))))
        ;; Note 0 index here to test subtyping in linking
        (global (import "m" "g") (ref null 0))
      )
    `, { m: { g: m1.exports.g } });
  }

  {
    let m1 = instantiate(`
      (module
        (type (sub (struct (field i32))))
        (type (sub 0 (struct (field i32))))
        (global (export "g") (ref null 1) (ref.null 1))
      )
    `);
    assert.throws(
      () => instantiate(`
        (module
          (type (sub (struct)))
          (type (sub 0 (struct (field i32))))
          (global (import "m" "g") (ref null 1))
        )
      `, { m: { g: m1.exports.g } }),
      WebAssembly.LinkError,
      "imported global m:g must be a same type"
    );
  }

  // Test subtyping between references where the parent is recursive and the subtyping depth is greater than one.
  instantiate(`
    (module
      (rec (type (sub (func (result (ref 0))))))
      (rec (type (sub 0 (func (result (ref 1))))))
      (type (sub 1 (func (result (ref 1)))))

      (func (result (ref null 0))
        (ref.null 2))
    )
  `);

  // This is an excerpt from the spec tests, which tests interactions between subtyping and recursion.
  instantiate(`
    (module
      (rec
        (type $t1 (sub (func (param i32 (ref $t3)))))
        (type $t2 (sub $t1 (func (param i32 (ref $t2)))))
        (type $t3 (sub $t2 (func (param i32 (ref $t1)))))
      )

      (func $f1 (param $r (ref $t1))
        (call $f1 (local.get $r)))
    )
  `);

  // Ensure a singleton recursive sub clause triggers structural subtype checking.
  assert.throws(
    () => instantiate(`
      (module
        (type (sub (struct (field i32))))
        (type (sub 0 (struct (field (ref 1)))))
      )
    `),
    WebAssembly.CompileError,
    "structural type is not a subtype"
  );

  // Ensure recursion group identity matters for subtyping.
  assert.throws(
    () => instantiate(`
      (module
        (rec (type $f1 (sub (func)))
             (type (struct))
             (type $s1 (sub $f1 (func))))
        (rec (type $f2 (sub (func)))
             (type $s2 (sub $f2 (func))))

        (func (param (ref null $f1)))
        (func (call 0 (ref.null $s2)))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: argument type mismatch in call, got (((() -> []), ((<current-rec-group>.0)() -> [])).1), expected (((() -> []), (), ((<current-rec-group>.0)() -> [])).0), in function at index 1"
  );

  // Check cases with a single non-recursive sub in a recursion group.
  // These tests are binary tests because it's difficult to ensure the
  // recursion group will show up in the binary encoding otherwise.
  //
  //  (module
  //    (rec (type (sub (struct (field i32)))))
  //    (rec (type (sub 0 (struct (field i32 i32)))))
  //  )
  module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x94\x80\x80\x80\x00\x02\x4e\x01\x50\x00\x5f\x01\x7f\x00\x4e\x01\x50\x01\x00\x5f\x02\x7f\x00\x7f\x00");

  //  (module
  //    (rec (type (sub (struct (field i32)))))
  //    (rec (type (sub 0 (struct (field)))))
  //  )
  assert.throws(
    () => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x02\x4e\x01\x50\x00\x5f\x01\x7f\x00\x4e\x01\x50\x01\x00\x5f\x00"),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 30: structural type is not a subtype of the specified supertype"
  )
}

function testFinalSubDeclaration() {
  instantiate(`
    (module
      (type (sub final (struct)))
    )
  `);

  instantiate(`
    (module
      (type (sub (struct)))
      (type (sub final (struct)))
    )
  `);

  assert.throws(
    () => instantiate(`
      (module
        (type (sub final (struct)))
        (type (sub 0 (struct)))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 22: cannot declare subtype of final supertype"
  );

  assert.throws(
    () => instantiate(`
      (module
        (type (struct))
        (type (sub 0 (struct)))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 22: cannot declare subtype of final supertype"
  );
}

testSubDeclaration();
testFinalSubDeclaration();
