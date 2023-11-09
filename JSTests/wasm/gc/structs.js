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

  instantiate(`
    (module
      (type (struct (field i32)))
      (func (result structref) (ref.null 0))
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

  /*
   * struct payload size overflows unsigned type
   * (module
   *   (type (struct (field (mut i64)) (field (mut i64)) ... (field (mut i64))))
   * )
   */
  assert.throws(
    () =>
      module(
        "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x07\x01\x5f\x80\x80\x2c\x7e\x01"
      ),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 15: number of fields for struct type at position 0 is too big 720896 maximum 10000 (evaluating 'new WebAssembly.Module(buffer)')"
  );

  // Invalid subtyping for structref.
  assert.throws(
    () =>
      compile(`
        (module
          (type (array i32))
          (func (result structref) (ref.null 0))
        )
      `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. (I32, mutable) is not a Structref, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  );

  // Invalid subtyping for structref.
  assert.throws(
    () =>
      compile(`
        (module
          (type (struct))
          (func (result structref) (ref.null 0))
          (func (result (ref null 0)) (call 0))
        )
      `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: control flow returns with unexpected type. Structref is not a (), in function at index 1 (evaluating 'new WebAssembly.Module(binary)')"
  );
}

function testStructJS() {
  // Wasm-allocated structs can flow to JS and back.
  {
    let m = instantiate(`
      (module
        (type (struct))
        (func (export "f") (result (ref null 0))
          (ref.null 0))
      )
    `);
    assert.eq(m.exports.f(), null);
  }

  {
    let m = instantiate(`
      (module
        (type (struct))
        (func (export "f") (param (ref null 0)))
      )
    `);
    m.exports.f(null);
  }

  // Test cast failure
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
    TypeError,
    "Argument value did not match reference type"
  )
}

function testStructNew() {
  {
    /*
     * (module
     *   (type $Empty (struct))
     *   (func (export "main")
     *     (drop
     *       (struct.new $Empty)
     *     )
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x06\x02\x60\x00\x00\x5f\x00\x03\x02\x01\x00\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x08\x01\x06\x00\xfb\x00\x01\x1a\x0b"));
    instance.exports.main();
  }

  instantiate(`
    (module
      (type $Empty (struct))
      (func (export "main")
        (drop
          (struct.new $Empty)
        )
      )
    )
  `).exports.main();

  instantiate(`
    (module
      ;; Also test with subtype.
      (type (sub (struct)))
      (type $Empty (sub 0 (struct)))
      (func (export "main")
        (drop
          (struct.new $Empty)
        )
      )
    )
  `).exports.main();

  {
    /*
     * (module
     *   (type $Point (struct (field $x i32) (field $y i32)))
     *   (func (export "main")
     *     (drop
     *       (struct.new $Point (i32.const 19) (i32.const 37))
     *     )
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0a\x02\x60\x00\x00\x5f\x02\x7f\x00\x7f\x00\x03\x02\x01\x00\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x0c\x01\x0a\x00\x41\x13\x41\x25\xfb\x00\x01\x1a\x0b"));
    instance.exports.main();

    /*
     * invalid type index for struct.new
     * (module
     *   (type $Point (struct (field $x i32) (field $y i32)))
     *   (func (export "main")
     *     (drop
     *       (struct.new (i32.const 19) (i32.const 37))
     *     )
     *   )
     * )
    */
    assert.throws(
      () =>
        module(
          "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0a\x02\x60\x00\x00\x5f\x02\x7f\x00\x7f\x00\x03\x02\x01\x00\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x0c\x01\x0a\x00\x41\x13\x41\x25\xfb\x00\x03\x1a\x0b"
        ),
      WebAssembly.CompileError,
      "WebAssembly.Module doesn't validate: struct.new index 3 is out of bound, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')"
    );
  }

  {
    /*
     * (module
     *   (type $Point (struct (field $x i32) (field $y i32)))
     *   (func (export "main")
     *     unreachable
     *     struct.new $Point (i32.const 19) (i32.const 37)
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0a\x02\x60\x00\x00\x5f\x02\x7f\x00\x7f\x00\x03\x02\x01\x00\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x0c\x01\x0a\x00\x00\x41\x13\x41\x25\xfb\x07\x01\x0b"));

    /*
     * invalid type index for struct.new in unreachable context
     * (module
     *   (type $Point (struct (field $x i32) (field $y i32)))
     *   (func (export "main")
     *     unreachable
     *     struct.new (i32.const 19) (i32.const 37)
     *   )
     * )
    */
    assert.throws(
      () =>
        module(
          "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0a\x02\x60\x00\x00\x5f\x02\x7f\x00\x7f\x00\x03\x02\x01\x00\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x0c\x01\x0a\x00\x00\x41\x13\x41\x25\xfb\x00\x02\x0b"
        ),
      WebAssembly.CompileError,
      "WebAssembly.Module doesn't validate: struct.new index 2 is out of bound, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')"
    );
  }

  instantiate(`
    (module
      (type $Point (struct (field $x i32) (field $y i32)))
      (func (export "main")
        unreachable
        struct.new $Point (i32.const 19) (i32.const 37)
      )
    )
  `);
}

function testStructNewDefault() {
  instantiate(`
    (module
      (type $Empty (struct))
      (func (export "main")
        (drop
          (struct.new_default $Empty)
        )
      )
    )
  `).exports.main();

  instantiate(`
    (module
      ;; Also test with subtype.
      (type (sub (struct)))
      (type $Empty (sub 0 (struct)))
      (func (export "main")
        (drop
          (struct.new_default $Empty)
        )
      )
    )
  `).exports.main();

  instantiate(`
     (module
       (type $Point (struct (field $x i32) (field $y i32)))
       (func (export "main")
         (drop
           (struct.new_default $Point)
         )
       )
     )
  `).exports.main();

  assert.throws(
    () => compile(`
            (module
              (type $Point (struct (field $x (ref func))))
              (func (export "main")
                (drop
                  (struct.new_default $Point)
                )
              )
            )
         `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 4: struct.new_default 0 requires all fields to be defaultable, but field 0 has type Ref, in function at index 0"
  )

  assert.throws(
    () => compile(`
            (module
              (type $Point (struct (field $x i32) (field $y i32)))
              (func (export "main")
                (drop
                  (struct.new_default 3)
                )
              )
            )
         `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: struct.new_default index 3 is out of bound, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  )

  assert.throws(
    () => compile(`
            (module
              (type $Point (struct (field $x i32) (field $y i32)))
              (func (export "main")
                unreachable
                struct.new_default 2
              )
            )
         `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: struct.new_default index 2 is out of bound, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  )
}

function testStructGet() {
  {
    /*
     * Point(i32)
     *
     * (module
     *   (type $Point (struct (field $x i32)))
     *    (func (export "main") (result i32)
     *      (struct.get $Point $x
     *        (struct.new $Point (i32.const 37))
     *      )
     *    )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x09\x02\x5f\x01\x7f\x00\x60\x00\x01\x7f\x03\x02\x01\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x0d\x01\x0b\x00\x41\x25\xfb\x00\x00\xfb\x02\x00\x00\x0b"));
    assert.eq(instance.exports.main(), 37);
  }

  {
    let main = instantiate(`
      (module
        (type $Point (struct (field $x i32)))
         (func (export "main") (result i32)
           (struct.get $Point $x
             (struct.new $Point (i32.const 37))
           )
         )
      )
    `).exports.main;
    assert.eq(main(), 37);
  }

  {
    let main = instantiate(`
      (module
        (type $Point (struct (field $x i32)))
         (func (export "main") (result i32)
           (struct.get $Point $x
             (struct.new_default $Point)
           )
         )
      )
    `).exports.main;
    assert.eq(main(), 0);
  }

  {
    let main = instantiate(`
      (module
        ;; Test subtype case as well.
        (type (sub (struct (field i32))))
        (type $Point (sub 0 (struct (field $x i32))))
         (func (export "main") (result i32)
           (struct.get $Point $x
             (struct.new $Point (i32.const 37))
           )
         )
      )
    `).exports.main;
    assert.eq(main(), 37);
  }

  {
    /*
     * Point(f32)
     *
     * (module
     *   (type $Point (struct (field $x f32)))
     *    (func (export "main") (result f32)
     *      (struct.get $Point $x
     *        (struct.new $Point (f32.const 37))
     *      )
     *    )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x09\x02\x5f\x01\x7d\x00\x60\x00\x01\x7d\x03\x02\x01\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x10\x01\x0e\x00\x43\x00\x00\x14\x42\xfb\x00\x00\xfb\x02\x00\x00\x0b"));
    assert.eq(instance.exports.main(), 37);
  }

  {
    let main = instantiate(`
      (module
        (type $Point (struct (field $x f32)))
         (func (export "main") (result f32)
           (struct.get $Point $x
             (struct.new_default $Point)
           )
         )
      )
    `).exports.main;
    assert.eq(main(), 0);
  }

  {
    /*
     * Point(i64)
     *
     * (module
     *   (type $Point (struct (field $x i64)))
     *   (func (export "main") (result i32)
     *     (i64.eq
     *       (i64.const 37)
     *       (struct.get $Point $x
     *         (struct.new $Point (i64.const 37))
     *       )
     *     )
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x09\x02\x5f\x01\x7e\x00\x60\x00\x01\x7f\x03\x02\x01\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x10\x01\x0e\x00\x42\x25\x42\x25\xfb\x00\x00\xfb\x02\x00\x00\x51\x0b"));
    assert.eq(instance.exports.main(), 1);
  }

  {
    let main = instantiate(`
      (module
        (type $Point (struct (field $x i64)))
        (func (export "main") (result i32)
          (i64.eq
            (i64.const 0)
            (struct.get $Point $x
              (struct.new_default $Point)
            )
          )
        )
      )
    `).exports.main;
    assert.eq(main(), 1);
  }

  {
    /*
     * Point(f64)
     *
     * (module
     *   (type $Point (struct (field $x f64)))
     *    (func (export "main") (result f64)
     *      (struct.get $Point $x
     *        (struct.new $Point (f64.const 37))
     *      )
     *    )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x09\x02\x5f\x01\x7c\x00\x60\x00\x01\x7c\x03\x02\x01\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x14\x01\x12\x00\x44\x00\x00\x00\x00\x00\x80\x42\x40\xfb\x00\x00\xfb\x02\x00\x00\x0b"));
    assert.eq(instance.exports.main(), 37);
  }

  {
    let main = instantiate(`
      (module
        (type $Point (struct (field $x f64)))
         (func (export "main") (result f64)
           (struct.get $Point $x
             (struct.new_default $Point)
           )
         )
      )
    `).exports.main;
    assert.eq(main(), 0);
  }

  {
    /*
     * Point(externref)
     *
     * (module
     *   (type $Point (struct (field $x externref)))
     *    (func (export "main") (param $obj externref) (result externref)
     *      (struct.get $Point $x
     *        (struct.new $Point (local.get $obj))
     *      )
     *    )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0a\x02\x5f\x01\x6f\x00\x60\x01\x6f\x01\x6f\x03\x02\x01\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x0d\x01\x0b\x00\x20\x00\xfb\x00\x00\xfb\x02\x00\x00\x0b"));
    let obj = {};
    assert.eq(instance.exports.main(obj), obj);
  }

  {
    let main = instantiate(`
      (module
        (type $Point (struct (field $x externref)))
        (func (export "main") (result externref)
          (struct.get $Point $x
            (struct.new_default $Point)
          )
        )
      )
    `).exports.main;
    assert.eq(main(), null);
  }

  {
    /*
     * (module
     *   (func (export "f") (param f64) (result f64) (local.get 0))
     * )
    */
    let instance1 = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x06\x01\x60\x01\x7c\x01\x7c\x03\x02\x01\x00\x07\x05\x01\x01\x66\x00\x00\x0a\x06\x01\x04\x00\x20\x00\x0b"));

    /*
     * Point(funcref)
     *
     * (module
     *   (type $Point (struct (field $x funcref)))
     *    (func (export "main") (param $p funcref) (result funcref)
     *      (struct.get $Point $x
     *        (struct.new $Point (local.get $p))
     *      )
     *    )
     * )
    */
    let instance2 = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0a\x02\x5f\x01\x70\x00\x60\x01\x70\x01\x70\x03\x02\x01\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x0d\x01\x0b\x00\x20\x00\xfb\x00\x00\xfb\x02\x00\x00\x0b"));
    let foo = instance1.exports.f;
    assert.eq(instance2.exports.main(foo), foo);
  }

  {
    let main = instantiate(`
      (module
        (type $Point (struct (field $x funcref)))
        (func (export "main") (result funcref)
          (struct.get $Point $x
            (struct.new_default $Point)
          )
        )
      )
    `).exports.main;
    assert.eq(main(), null);
  }

  {
    /*
     * Point(i32, i32)
     *
     * (module
     *   (type $Point (struct (field $x i32) (field $y i32)))
     *    (func (export "main") (result i32)
     *      (struct.get $Point $x
     *        (struct.new $Point (i32.const 19) (i32.const 37))
     *      )
     *    )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0b\x02\x5f\x02\x7f\x00\x7f\x00\x60\x00\x01\x7f\x03\x02\x01\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x0f\x01\x0d\x00\x41\x13\x41\x25\xfb\x00\x00\xfb\x02\x00\x00\x0b"));
    assert.eq(instance.exports.main(), 19);
  }

  {
    /*
     * Point(f32, f32)
     *
     * (module
     *   (type $Point (struct (field $x f32) (field $y f32)))
     *    (func (export "main") (result f32)
     *      (struct.get $Point $x
     *        (struct.new $Point (f32.const 19) (f32.const 37))
     *      )
     *    )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0b\x02\x5f\x02\x7d\x00\x7d\x00\x60\x00\x01\x7d\x03\x02\x01\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x15\x01\x13\x00\x43\x00\x00\x98\x41\x43\x00\x00\x14\x42\xfb\x00\x00\xfb\x02\x00\x00\x0b"));
    assert.eq(instance.exports.main(), 19);
  }

  {
    /*
     * Point(i32, f64)
     *
     * (module
     *   (type $Point (struct (field $x i32) (field $y f64)))
     *    (func (export "get_x") (result i32)
     *      (struct.get $Point $x
     *        (struct.new $Point (i32.const 19) (f64.const 37))
     *      )
     *    )
     *    (func (export "get_y") (result f64)
     *      (struct.get $Point $y
     *        (struct.new $Point (i32.const 19) (f64.const 37))
     *      )
     *    )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0f\x03\x5f\x02\x7f\x00\x7c\x00\x60\x00\x01\x7f\x60\x00\x01\x7c\x03\x03\x02\x01\x02\x07\x11\x02\x05\x67\x65\x74\x5f\x78\x00\x00\x05\x67\x65\x74\x5f\x79\x00\x01\x0a\x2b\x02\x14\x00\x41\x13\x44\x00\x00\x00\x00\x00\x80\x42\x40\xfb\x00\x00\xfb\x02\x00\x00\x0b\x14\x00\x41\x13\x44\x00\x00\x00\x00\x00\x80\x42\x40\xfb\x00\x00\xfb\x02\x00\x01\x0b"));
    assert.eq(instance.exports.get_x(), 19);
    assert.eq(instance.exports.get_y(), 37);
  }

  {
    // unreachable context

    /*
     * (module
     *   (type $Point (struct (field $x i32) (field $y i32)))
     *   (func (export "main") (result i32)
     *     unreachable
     *     (struct.get $Point $x
     *       (struct.new $Point (i32.const 19) (i32.const 37))
     *     )
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0b\x02\x5f\x02\x7f\x00\x7f\x00\x60\x00\x01\x7f\x03\x02\x01\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x10\x01\x0e\x00\x00\x41\x13\x41\x25\xfb\x00\x00\xfb\x02\x00\x00\x0b"));

    /*
     * invalid type index for struct.get in unreachable context
     *
     * (module
     *   (type $Point (struct (field $x i32) (field $y i32)))
     *   (func (export "main") (result i32)
     *     unreachable
     *     (struct.get $Point $x
     *       (struct.new $Point (i32.const 19) (i32.const 37))
     *     )
     *   )
     * )
    */
    assert.throws(
      () =>
        module(
          "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0b\x02\x5f\x02\x7f\x00\x7f\x00\x60\x00\x01\x7f\x03\x02\x01\x01\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x00\x0a\x10\x01\x0e\x00\x00\x41\x13\x41\x25\xfb\x00\x00\xfb\x02\x03\x00\x0b"
        ),
      WebAssembly.CompileError,
      "WebAssembly.Module doesn't validate: struct.get index 3 is out of bound, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')"
    );
  }

  // Test error message for invalid struct.get index.
  assert.throws(
    () =>
      compile(`
        (module
          (func (result i32) (struct.get 5 0 (ref.null 0)))
        )
      `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: struct.get index 5 is out of bound, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  );

  // Test error message for invalid struct.get index.
  assert.throws(
    () =>
      compile(`
        (module
          (type (func))
          (func (result i32) (struct.get 0 0 (ref.null 0)))
        )
      `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: struct.get: invalid type index 0, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  );

  // Cannot struct.get from a structref.
  assert.throws(
    () =>
      compile(`
        (module
          (type $s (struct (field i32)))
          (func (result structref) (ref.null 0))
          (func (result i32) (struct.get 0 0 (call 0)))
        )
      `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: struct.get invalid index: Structref, in function at index 1 (evaluating 'new WebAssembly.Module(binary)')"
  );

  // Test null checks.
  assert.throws(
    () => instantiate(`
      (module
        (type (struct (field i32)))
        (func (export "f") (result i32)
          (struct.get 0 0 (ref.null 0)))
      )
    `).exports.f(),
    WebAssembly.RuntimeError,
    "struct.get to a null reference"
  );
}

function testStructSet() {
  {
    /*
     * Point(i32)
     *
     * (module
     *   (type $Point (struct (field $x (mut i32))))
     *   (func $doTest (param $p (ref $Point)) (result i32)
     *     (struct.set $Point $x
     *       (local.get $p)
     *       (i32.const 37)
     *     )
     *     (struct.get $Point $x
     *       (local.get $p)
     *     )
     *   )
     *
     *   (func (export "main") (result i32)
     *     (call $doTest
     *       (struct.new $Point (i32.const 0))
     *     )
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0f\x03\x5f\x01\x7f\x01\x60\x01\x64\x00\x01\x7f\x60\x00\x01\x7f\x03\x03\x02\x01\x02\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x0a\x1c\x02\x10\x00\x20\x00\x41\x25\xfb\x05\x00\x00\x20\x00\xfb\x02\x00\x00\x0b\x09\x00\x41\x00\xfb\x00\x00\x10\x00\x0b"));
    assert.eq(instance.exports.main(), 37);
  }

  {
    let main = instantiate(`
      (module
        (type $Point (struct (field $x (mut i32))))
        (func $doTest (param $p (ref $Point)) (result i32)
          (struct.set $Point $x
            (local.get $p)
            (i32.const 37)
          )
          (struct.get $Point $x
            (local.get $p)
          )
        )

        (func (export "main") (result i32)
          (call $doTest
            (struct.new $Point (i32.const 0))
          )
        )
      )
    `).exports.main;
    assert.eq(main(), 37);
  }

  {
    let main = instantiate(`
      (module
        ;; Test subtype case as well.
        (type (sub (struct (field (mut i32)))))
        (type $Point (sub 0 (struct (field $x (mut i32)))))
        (func $doTest (param $p (ref $Point)) (result i32)
          (struct.set $Point $x
            (local.get $p)
            (i32.const 37)
          )
          (struct.get $Point $x
            (local.get $p)
          )
        )

        (func (export "main") (result i32)
          (call $doTest
            (struct.new $Point (i32.const 0))
          )
        )
      )
    `).exports.main;
    assert.eq(main(), 37);
  }

  // Test actually passing a point that is a subtype to a $Point interface.
  {
    let main = instantiate(`
      (module
        (type $Point (sub (struct (field $x (mut i32)))))
        (type $Sub (sub 0 (struct (field (mut i32) (mut i32)))))
        (func $doTest (result i32) (local $p (ref null $Sub))
          (local.set $p (struct.new $Sub (i32.const 0) (i32.const 1)))
          (struct.set $Point $x
            (local.get $p)
            (i32.const 37)
          )
          (struct.get $Point $x
            (local.get $p)
          )
        )

        (func (export "main") (result i32)
          (call $doTest)
        )
      )
    `).exports.main;
    assert.eq(main(), 37);
  }

  {
    /*
     * Point(f32)
     *
     * (module
     *   (type $Point (struct (field $x (mut f32))))
     *   (func $doTest (param $p (ref $Point)) (result f32)
     *     (struct.set $Point $x
     *       (local.get $p)
     *       (f32.const 37)
     *     )
     *     (struct.get $Point $x
     *       (local.get $p)
     *     )
     *   )
     *
     *   (func (export "main") (result f32)
     *     (call $doTest
     *       (struct.new $Point (f32.const 0))
     *     )
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0f\x03\x5f\x01\x7d\x01\x60\x01\x64\x00\x01\x7d\x60\x00\x01\x7d\x03\x03\x02\x01\x02\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x0a\x22\x02\x13\x00\x20\x00\x43\x00\x00\x14\x42\xfb\x05\x00\x00\x20\x00\xfb\x02\x00\x00\x0b\x0c\x00\x43\x00\x00\x00\x00\xfb\x00\x00\x10\x00\x0b"));
    assert.eq(instance.exports.main(), 37);
  }

  {
    /*
     * Point(i64)
     *
     * (module
     *   (type $Point (struct (field $x (mut i64))))
     *   (func $doTest (param $p (ref $Point)) (result i64)
     *     (struct.set $Point $x
     *       (local.get $p)
     *       (i64.const 37)
     *     )
     *     (struct.get $Point $x
     *       (local.get $p)
     *     )
     *   )
     *
     *   (func (export "main") (result i64)
     *     (call $doTest
     *       (struct.new $Point (i64.const 0))
     *     )
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0f\x03\x5f\x01\x7e\x01\x60\x01\x64\x00\x01\x7e\x60\x00\x01\x7e\x03\x03\x02\x01\x02\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x0a\x1c\x02\x10\x00\x20\x00\x42\x25\xfb\x05\x00\x00\x20\x00\xfb\x02\x00\x00\x0b\x09\x00\x42\x00\xfb\x00\x00\x10\x00\x0b"));
    assert.eq(instance.exports.main() == 37, true);
  }

  {
    /*
     * Point(f64)
     *
     * (module
     *   (type $Point (struct (field $x (mut f64))))
     *   (func $doTest (param $p (ref $Point)) (result f64)
     *     (struct.set $Point $x
     *       (local.get $p)
     *       (f64.const 37)
     *     )
     *     (struct.get $Point $x
     *       (local.get $p)
     *     )
     *   )
     *
     *   (func (export "main") (result f64)
     *     (call $doTest
     *       (struct.new $Point (f64.const 0))
     *     )
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0f\x03\x5f\x01\x7c\x01\x60\x01\x64\x00\x01\x7c\x60\x00\x01\x7c\x03\x03\x02\x01\x02\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x0a\x2a\x02\x17\x00\x20\x00\x44\x00\x00\x00\x00\x00\x80\x42\x40\xfb\x05\x00\x00\x20\x00\xfb\x02\x00\x00\x0b\x10\x00\x44\x00\x00\x00\x00\x00\x00\x00\x00\xfb\x00\x00\x10\x00\x0b"));
    assert.eq(instance.exports.main(), 37);
  }

  {
    /*
     * Point(externref)
     *
     * (module
     *   (type $Point (struct (field $x (mut externref))))
     *   (func $doSet (param $p (ref $Point)) (param $obj externref) (result externref)
     *     (struct.set $Point $x
     *       (local.get $p)
     *       (local.get $obj)
     *     )
     *     (struct.get $Point $x
     *       (local.get $p)
     *     )
     *   )
     *
     *   (func (export "main") (param $obj externref) (result externref)
     *     (call $doSet
     *       (struct.new $Point (ref.null extern))
     *       (local.get $obj)
     *     )
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x11\x03\x5f\x01\x6f\x01\x60\x02\x64\x00\x6f\x01\x6f\x60\x01\x6f\x01\x6f\x03\x03\x02\x01\x02\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x0a\x1e\x02\x10\x00\x20\x00\x20\x01\xfb\x05\x00\x00\x20\x00\xfb\x02\x00\x00\x0b\x0b\x00\xd0\x6f\xfb\x00\x00\x20\x00\x10\x00\x0b"));
    let obj = {};
    assert.eq(instance.exports.main(obj), obj);
  }

  {
    /*
     * (module
     *   (func (export "f") (param f64) (result f64) (local.get 0))
     * )
    */
    let instance1 = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x06\x01\x60\x01\x7c\x01\x7c\x03\x02\x01\x00\x07\x05\x01\x01\x66\x00\x00\x0a\x06\x01\x04\x00\x20\x00\x0b"));

    /*
     * Point(funcref)
     *
     * (module
     *   (type $Point (struct (field $x (mut funcref))))
     *   (func $doSet (param $p (ref $Point)) (param $obj funcref) (result funcref)
     *     (struct.set $Point $x
     *       (local.get $p)
     *       (local.get $obj)
     *     )
     *     (struct.get $Point $x
     *       (local.get $p)
     *     )
     *   )
     *
     *   (func (export "main") (param $obj funcref) (result funcref)
     *     (call $doSet
     *       (struct.new $Point (ref.null func))
     *       (local.get $obj)
     *     )
     *   )
     * )
    */
    let instance2 = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x11\x03\x5f\x01\x70\x01\x60\x02\x64\x00\x70\x01\x70\x60\x01\x70\x01\x70\x03\x03\x02\x01\x02\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x0a\x1e\x02\x10\x00\x20\x00\x20\x01\xfb\x05\x00\x00\x20\x00\xfb\x02\x00\x00\x0b\x0b\x00\xd0\x70\xfb\x00\x00\x20\x00\x10\x00\x0b"));
    let foo = instance1.exports.f;
    assert.eq(instance2.exports.main(foo), foo);
  }

  {
    /*
     * Point(i32, i32)
     *
     * (module
     *   (type $Point (struct (field $x (mut i32)) (field $y (mut i32))))
     *   (func $doSet (param $p (ref $Point)) (param $val i32) (result i32)
     *     (struct.set $Point $y
     *       (local.get $p)
     *       (local.get $val)
     *     )
     *     (struct.get $Point $y
     *       (local.get $p)
     *     )
     *   )
     *
     *   (func (export "main") (result i32)
     *     (call $doSet
     *       (struct.new $Point (i32.const 0) (i32.const 0))
     *       (i32.const 19)
     *     )
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x12\x03\x5f\x02\x7f\x01\x7f\x01\x60\x02\x64\x00\x7f\x01\x7f\x60\x00\x01\x7f\x03\x03\x02\x01\x02\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x0a\x20\x02\x10\x00\x20\x00\x20\x01\xfb\x05\x00\x01\x20\x00\xfb\x02\x00\x01\x0b\x0d\x00\x41\x00\x41\x00\xfb\x00\x00\x41\x13\x10\x00\x0b"));
    assert.eq(instance.exports.main(), 19);
  }

  {
    /*
     * Point(i32, i64)
     *
     * (module
     *   (type $Point (struct (field $x (mut i32)) (field $y (mut i64))))
     *   (func $doSet (param $p (ref $Point)) (param $val i32) (result i32)
     *     (struct.set $Point $x
     *       (local.get $p)
     *       (local.get $val)
     *     )
     *     (struct.get $Point $x
     *       (local.get $p)
     *     )
     *   )
     *
     *   (func (export "main") (result i32)
     *     (call $doSet
     *       (struct.new $Point (i32.const 0) (i64.const 0))
     *       (i32.const 19)
     *     )
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x12\x03\x5f\x02\x7f\x01\x7e\x01\x60\x02\x64\x00\x7f\x01\x7f\x60\x00\x01\x7f\x03\x03\x02\x01\x02\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x0a\x20\x02\x10\x00\x20\x00\x20\x01\xfb\x05\x00\x00\x20\x00\xfb\x02\x00\x00\x0b\x0d\x00\x41\x00\x42\x00\xfb\x00\x00\x41\x13\x10\x00\x0b"));
    assert.eq(instance.exports.main(), 19);
  }

  {
    /*
     * trying to set a non-mutable field
     *
     * (module
     *   (type $Point (struct (field $x (i32))))
     *   (func $doTest (param $p (ref $Point)) (result i32)
     *     (struct.set $Point $x
     *       (local.get $p)
     *       (i32.const 37)
     *     )
     *     (struct.get $Point $x
     *       (local.get $p)
     *     )
     *   )
     *
     *   (func (export "main") (result i32)
     *     (call $doTest
     *       (struct.new $Point (i32.const 0))
     *     )
     *   )
     * )
    */
    assert.throws(
      () =>
        module(
          "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0f\x03\x5f\x01\x7f\x00\x60\x01\x64\x00\x01\x7f\x60\x00\x01\x7f\x03\x03\x02\x01\x02\x07\x08\x01\x04\x6d\x61\x69\x6e\x00\x01\x0a\x1c\x02\x10\x00\x20\x00\x41\x25\xfb\x05\x00\x00\x20\x00\xfb\x02\x00\x00\x0b\x09\x00\x41\x00\xfb\x00\x00\x10\x00\x0b"
        ),
      WebAssembly.CompileError,
      "WebAssembly.Module doesn't parse at byte 9: the field 0 can't be set because it is immutable, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')"
    );
  }

  {
    // unreachable context

    /*
     *
     * (module
     *   (type $Point (struct (field $x (mut i32))))
     *   (func $doTest (param $p (ref $Point)) (result i32)
     *     (unreachable)
     *     (struct.set $Point $x
     *       (local.get $p)
     *       (i32.const 37)
     *     )
     *     (struct.get $Point $x
     *       (local.get $p)
     *     )
     *   )
     * )
    */
    let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0b\x02\x5f\x01\x7f\x01\x60\x01\x64\x00\x01\x7f\x03\x02\x01\x01\x0a\x13\x01\x11\x00\x00\x20\x00\x41\x25\xfb\x05\x00\x00\x20\x00\xfb\x02\x00\x00\x0b"));

    /*
     * invalid type index for struct.set in unreachable context
     *
     * (module
     *   (type $Point (struct (field $x (mut i32))))
     *   (func $doTest (param $p (ref $Point)) (result i32)
     *     (unreachable)
     *     (struct.set $Point $x
     *       (local.get $p)
     *       (i32.const 37)
     *     )
     *     (struct.get $Point $x
     *       (local.get $p)
     *     )
     *   )
     * )
    */
    assert.throws(
      () =>
        module(
          "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0b\x02\x5f\x01\x7f\x01\x60\x01\x64\x00\x01\x7f\x03\x02\x01\x01\x0a\x13\x01\x11\x00\x00\x20\x00\x41\x25\xfb\x05\x00\x00\x20\x00\xfb\x02\x05\x00\x0b"
        ),
      WebAssembly.CompileError,
      "WebAssembly.Module doesn't validate: struct.get index 5 is out of bound, in function at index 0 (evaluating 'new WebAssembly.Module(buffer)')"
    );
  }

  // Test error message for invalid struct.set index.
  assert.throws(
    () =>
      compile(`
        (module
          (func (result i32) (struct.set 5 0 (ref.null 0) (i32.const 42)))
        )
      `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: struct.set index 5 is out of bound, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  );

  // Test null checks.
  assert.throws(
    () => instantiate(`
      (module
        (type (struct (field (mut i32))))
        (func (export "f") (result)
          (struct.set 0 0 (ref.null 0) (i32.const 42)))
      )
    `).exports.f(),
    WebAssembly.RuntimeError,
    "struct.set to a null reference"
  );
}

function testStructTable() {
  {
    let m = instantiate(`
      (module
        (type (struct (field i32)))
        (table 10 (ref null 0))
        (func (export "set") (param i32) (result)
          (table.set (local.get 0) (struct.new 0 (i32.const 42))))
        (func (export "get") (param i32) (result i32)
          (struct.get 0 0 (table.get (local.get 0)))
          )
      )
    `);
    m.exports.set(2);
    assert.eq(m.exports.get(2), 42);
    assert.throws(() => m.exports.get(4), WebAssembly.RuntimeError, "struct.get to a null");
  }

  // Invalid struct.get, needs a downcast.
  assert.throws(
    () => compile(`
      (module
        (type (struct (field i32)))
        (table 10 (ref null struct))
        (func (param i32) (result i32)
          (struct.get 0 0 (table.get (local.get 0))))
      )
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't validate: struct.get invalid index: Structref, in function at index 0 (evaluating 'new WebAssembly.Module(binary)')"
  );

  // Invalid non-defaultable table type.
  assert.throws(
    () => compile(`
      (module
        (type (struct))
        (table 0 (ref 0)))
    `),
    WebAssembly.CompileError,
    "WebAssembly.Module doesn't parse at byte 26: Table's type must be defaultable (evaluating 'new WebAssembly.Module(binary)')"
  )

  // Test JS API.
  {
    const m = instantiate(`
      (module
        (type (struct))
        (table (export "t") 10 (ref null 0))
        (start 0)
        (func
          (table.fill (i32.const 0) (struct.new 0) (i32.const 10)))
        (func (export "makeStruct") (result (ref 0)) (struct.new 0)))
    `);
    const m2 = instantiate(`
      (module
        (type (array i32))
        (func (export "makeArray") (result (ref 0)) (array.new_default 0 (i32.const 5))))
    `);
    assert.eq(m.exports.t.get(0) !== null, true);
    assert.throws(
      () => m.exports.t.set(0, "foo"),
      TypeError,
      "WebAssembly.Table.prototype.set failed to cast the second argument to the table's element type"
    );
    assert.throws(
      () => m.exports.t.set(0, 3),
      TypeError,
      "WebAssembly.Table.prototype.set failed to cast the second argument to the table's element type"
    );
    assert.throws(
      () => m.exports.t.set(0, m2.exports.makeArray()),
      TypeError,
      "WebAssembly.Table.prototype.set failed to cast the second argument to the table's element type"
    );
    const str = m.exports.makeStruct();
    m.exports.t.set(0, str);
    assert.eq(m.exports.t.get(0), str);
    m.exports.t.set(0, null);
    assert.eq(m.exports.t.get(0), null);
  }
}

testStructDeclaration();
testStructJS();
testStructNew();
testStructNewDefault();
testStructGet();
testStructSet();
testStructTable();
