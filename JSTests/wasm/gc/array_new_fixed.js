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

function testArrayNewFixedImmutable() {
    let m = instantiate(`(module
  (type $vec (array f32))

  (func $new (export "new") (result (ref $vec))
    (array.new_fixed $vec 2 (f32.const 1) (f32.const 2))
  )

  (func $get (param $i i32) (param $v (ref $vec)) (result f32)
    (array.get $vec (local.get $v) (local.get $i))
  )
  (func (export "get") (param $i i32) (result f32)
    (call $get (local.get $i) (call $new))
  )

)`);

    // Can't test this because of https://bugs.webkit.org/show_bug.cgi?id=246769
    // assert.eq(typeof(m.exports["new"]()), "object");
    assert.eq(m.exports.get(0), 1.0);
}

function testArrayFuncrefs() {
    let m = instantiate(`(module
  (type $fty (func (param i32) (result i32)))
  (type $a (array (ref $fty)))

  (func $f (export "f") (param $i i32) (result i32)
     (local.get $i)
     (i32.const 1)
     (i32.add))

  (func (export "get") (param $i i32) (result (ref $fty))
    (array.new_fixed $a 2 (ref.func $f) (ref.func $f))
    (local.get $i)
    (array.get $a))

  (func (export "get_and_call") (param $i i32) (result i32)
    (i32.const 5)
    (array.new_fixed $a 2 (ref.func $f) (ref.func $f))
    (local.get $i)
    (array.get $a)
    (call_ref $fty)))`);

    assert.eq(typeof(m.exports.get(0)), "function");
    assert.eq(m.exports.get_and_call(0), 6);
    assert.eq(m.exports.get_and_call(1), 6);
}

function testArrayExternrefs() {
    let m = instantiate(`(module
  (type $fty (func (param i32) (result i32)))
  (type $a (array externref))

  (func $f (export "f") (param $i i32) (result i32)
     (local.get $i)
     (i32.const 1)
     (i32.add))

  (func $new (result (ref $a))
    (array.new_fixed $a 2 (ref.null extern) (ref.null extern)))

  (func (export "get") (param $i i32) (result externref)
    (call $new)
    (local.get $i)
    (array.get $a))

  (func (export "len") (result i32)
     (call $new)
     (array.len)))`);

    m.exports.get(0);
    m.exports.get(1);
    assert.eq(m.exports.len(), 2);
}

function testArrayRefNull() {
    let m = instantiate(`(module
  (type $fty (func (param i32) (result i32)))
  (type $a (array (ref null $fty)))

  (func $f (export "f") (param $i i32) (result i32)
     (local.get $i)
     (i32.const 1)
     (i32.add))

  (func $new (result (ref $a))
    (array.new_fixed $a 3 (ref.func $f) (ref.null $fty) (ref.func $f)))

  (func (export "get") (param $i i32) (result (ref null $fty))
    (call $new)
    (local.get $i)
    (array.get $a))

  (func (export "len") (result i32)
     (call $new)
     (array.len)))`);

    m.exports.get(0);
    assert.eq(m.exports.get(1), null);
    m.exports.get(2);
    assert.eq(m.exports.len(), 3);
}

function testZeroLengthArray() {
    let m = instantiate(`(module
  (type $vec (array i32))

  (func $new (export "new") (result (ref $vec))
    (array.new_fixed $vec 0)
  )

  (func $get (param $i i32) (param $v (ref $vec)) (result i32)
    (array.get $vec (local.get $v) (local.get $i))
  )
  (func (export "get") (param $i i32) (result i32)
    (call $get (local.get $i) (call $new))
  )

  (func $len (param $v (ref array)) (result i32)
    (array.len (local.get $v))
  )
  (func (export "len") (result i32)
    (call $len (call $new))
  )
)`);

    assert.throws(() => m.exports.get(0), WebAssembly.RuntimeError, "Out of bounds array.get");
    assert.eq(m.exports.len(), 0);
}

function testSingletonArray() {
    let m = instantiate(`(module
  (type $vec (array i32))

  (func $new (export "new") (result (ref $vec))
    (array.new_fixed $vec 1 (i32.const 42))
  )

  (func $get (param $i i32) (param $v (ref $vec)) (result i32)
    (array.get $vec (local.get $v) (local.get $i))
  )
  (func (export "get") (param $i i32) (result i32)
    (call $get (local.get $i) (call $new))
  )

  (func $len (param $v (ref array)) (result i32)
    (array.len (local.get $v))
  )
  (func (export "len") (result i32)
    (call $len (call $new))
  )
)`);

    assert.eq(m.exports.get(0), 42);
    assert.throws(() => m.exports.get(1), WebAssembly.RuntimeError, "Out of bounds array.get");
    assert.eq(m.exports.len(), 1);
}

function testTypeMismatch() {
    let moduleString = `(module
  (type $vec (array i32))

  (func $new (export "new") (result (ref $vec))
    (array.new_fixed $vec 3 (f32.const 0) (f32.const 1) (f32.const 2))
  ))`;

    assert.throws(() => compile(moduleString), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: argument type mismatch in array.new_fixed, got F32, expected a subtype of I32, in function at index 0");
}

function testArgLengthMismatch() {
    let noArgs = `(module
  (type $vec (array i32))

  (func $new (export "new") (result (ref $vec))
    (array.new_fixed $vec 1)
  ))`;

    assert.throws(() => compile(noArgs), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: array_new_fixed: found 0 operands on stack; expected 1 operands, in function at index 0");

    let tooFewArgs = `(module
  (type $vec (array i32))

  (func $new (export "new") (result (ref $vec))
    (array.new_fixed $vec 3 (f32.const 1) (f32.const 2))
  ))`;

    assert.throws(() => compile(tooFewArgs), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: array_new_fixed: found 2 operands on stack; expected 3 operands, in function at index 0");

    let tooManyArgs = `(module
  (type $vec (array i32))

  (func $new (export "new") (result (ref $vec))
    (array.new_fixed $vec 2 (i32.const 1) (i32.const 2) (i32.const 0) (i32.const 3))
  ))`;

    // The `array.fixed` operation itself isn't erroneous, but the additional arguments shouldn't be consumed when parsing it
  assert.throws(() => compile(tooManyArgs), WebAssembly.CompileError, "WebAssembly.Module doesn't validate:  block with type: () -> [Ref] returns: 1 but stack has: 3 values, in function at index 0");

    let nonEmptyStack = instantiate(`(module
  (type $vec (array f32))

  (func $new (result (ref null $vec))
    (local (ref null $vec))
    (array.new_fixed $vec 2 (f32.const 1) (f32.const 2) (f32.const 0) (f32.const 3.14))
    (local.set 0)
    (drop)
    (drop)
    (local.get 0))

  (func (export "len") (result i32)
      (call $new)
      (array.len)))`);

    // This should be valid, since the additional args are consumed
    assert.eq(nonEmptyStack.exports.len(), 2);
}

function testInvalidArrayType() {
    let nonExistentType = `(module

  (func $new (export "new") (result)
    (array.new_fixed 0 2 (f32.const 1) (f32.const 2))
    (drop)
  ))`;

    let errorMessage = "WebAssembly.Module doesn't validate: array.new_fixed index 0 does not reference an array definition, in function at index 0";

    assert.throws(() => compile(nonExistentType), WebAssembly.CompileError, errorMessage);
    let nonArrayType = `(module
       (type $s (struct (field i32) (field i32)))
  (func $new (export "new") (result (ref $s))
    (array.new_fixed $s 2 (f32.const 1) (f32.const 2))
  ))`;

    assert.throws(() => compile(nonArrayType), WebAssembly.CompileError, errorMessage);
}

function testSubtyping() {
    let m = instantiate(`(module
  (type $a (array funcref))
  (type $fty (func (result i32)))
  (type $gty (func (param i32) (result i32)))
  (func $f (export "f") (result i32) (i32.const 5))
  (func $g (export "g") (param $i i32) (result i32) (local.get $i))

  (func $new (export "new") (result (ref $a))
    (array.new_fixed $a 4 (ref.func $f) (ref.func $g) (ref.func $f) (ref.func $g))
  )

  (func $get (param $i i32) (param $v (ref $a)) (result funcref)
    (array.get $a (local.get $v) (local.get $i))
  )
  (func (export "get") (param $i i32) (result funcref)
    (call $get (local.get $i) (call $new))
  ))`);

    assert.eq(typeof(m.exports.get()), "function");
}

function testNestedArrays() {
   let m = instantiate(`(module
    (type $bvec (array i8))
    (type $a (array (ref $bvec)))

    (func $new (export "new") (result (ref $a))
       (array.new_fixed $a 4 (array.new_default $bvec (i32.const 2))
                             (array.new_default $bvec (i32.const 55))
                             (array.new_default $bvec (i32.const 0))
                             (array.new_default $bvec (i32.const 1))))

    (func (export "get_len") (param $i i32) (result i32)
      (call $new)
      (array.len (array.get $a (local.get $i)))))`);

    assert.eq(m.exports.get_len(0), 2);
    assert.eq(m.exports.get_len(1), 55);
    assert.eq(m.exports.get_len(2), 0);
    assert.eq(m.exports.get_len(3), 1);
}

function testPackedTypes()
{
    {
        let m = instantiate(`(module
          (type $a (array i8))

          (func $new (export "new") (result (ref $a))
            (array.new_fixed $a 3 (i32.const 1) (i32.const 2) (i32.const 65536)))

          (func $get (param $i i32) (param $v (ref $a)) (result i32)
            (array.get_u $a (local.get $v) (local.get $i)))

          (func (export "get") (param $i i32) (result i32)
            (call $get (local.get $i) (call $new))))`);

        assert.eq(m.exports.get(0), 1);
        assert.eq(m.exports.get(1), 2);
        assert.eq(m.exports.get(2), 0);
    }
    {
        let num = 0x100001;
        let m = instantiate(`(module
          (type $a (array i16))

          (func $new (export "new") (result (ref $a))
            (array.new_fixed $a 3 (i32.const 1) (i32.const 2) (i32.const ` + num + `)))

          (func $get (param $i i32) (param $v (ref $a)) (result i32)
            (array.get_u $a (local.get $v) (local.get $i)))

          (func (export "get") (param $i i32) (result i32)
            (call $get (local.get $i) (call $new))))`);

        assert.eq(m.exports.get(0), 1);
        assert.eq(m.exports.get(1), 2);
        assert.eq(m.exports.get(2), 1);
    }
}

function testNumericTypes() {
    {
        let num = 0x100000000 - 1;

        let m = instantiate(`(module
          (type $a (array i32))

          (func $new (export "new") (result (ref $a))
            (array.new_fixed $a 3 (i32.const 1) (i32.const 2) (i32.const ` + num + `)))

          (func $get (param $i i32) (param $v (ref $a)) (result i32)
            (array.get $a (local.get $v) (local.get $i)))

          (func (export "get") (param $i i32) (result i32)
            (call $get (local.get $i) (call $new))))`);

        assert.eq(m.exports.get(0), 1);
        assert.eq(m.exports.get(1), 2);
        assert.eq(m.exports.get(2), -1);
    }
    {
        let m = instantiate(`(module
          (type $a (array i64))

          (func $new (export "new") (result (ref $a))
            (array.new_fixed $a 3 (i64.const 1) (i64.const 2) (i64.const -5)))

          (func $get (param $i i32) (param $v (ref $a)) (result i64)
            (array.get $a (local.get $v) (local.get $i)))

          (func (export "get") (param $i i32) (result i64)
            (call $get (local.get $i) (call $new))))`);

        assert.eq(m.exports.get(0), BigInt(1));
        assert.eq(m.exports.get(1), BigInt(2));
        assert.eq(m.exports.get(2), BigInt(-5));
    }
    {
        let m = instantiate(`(module
         (type $a (array f32))

         (func $new (export "new") (result (ref $a))
           (array.new_fixed $a 3 (f32.const 1) (f32.const 3.14) (f32.const -4.2)))

         (func $get (param $i i32) (param $v (ref $a)) (result f32)
           (array.get $a (local.get $v) (local.get $i)))

         (func (export "get") (param $i i32) (result f32)
           (call $get (local.get $i) (call $new))))`);

      assert.eq(m.exports.get(0), 1);
      let get1 = m.exports.get(1);
      assert.eq(get1 >= 3.14 && get1 < 3.15, true);
      let get2 = m.exports.get(2);
      assert.eq(get2 < -4.19 && get2 >= -4.2, true);
    }
    {
        let m = instantiate(`(module
          (type $a (array f64))

          (func $new (export "new") (result (ref $a))
            (array.new_fixed $a 3 (f64.const 1) (f64.const 3.14) (f64.const -4.2)))

          (func $get (param $i i32) (param $v (ref $a)) (result f64)
            (array.get $a (local.get $v) (local.get $i)))

          (func (export "get") (param $i i32) (result f64)
            (call $get (local.get $i) (call $new))))`);

        assert.eq(m.exports.get(0), 1);
        let get1 = m.exports.get(1);
        assert.eq(get1, 3.14);
        let get2 = m.exports.get(2);
        assert.eq(get2, -4.2);
    }
}

function testStructs() {
   let m = instantiate(`(module
  (type $bvec (array i8))
  (type $s (struct (field i32) (field (ref $bvec)) (field f64)))
  (type $a (array (ref $s)))

  (func $new (export "new") (result (ref $a))
    (array.new_fixed $a 4 (struct.new $s (i32.const 3) (array.new_default $bvec (i32.const 1)) (f64.const 100.1))
                          (struct.new $s (i32.const 42) (array.new_default $bvec (i32.const 6)) (f64.const 0.0))
                          (struct.new $s (i32.const 17) (array.new_default $bvec (i32.const 7)) (f64.const -1.25))
                          (struct.new $s (i32.const 5) (array.new_default $bvec (i32.const 2)) (f64.const 3.14))))

  (func (export "get_field0") (param $i i32) (result i32)
    (call $new)
    (struct.get $s 0 (array.get $a (local.get $i))))

  (func (export "get_field1_len") (param $i i32) (result i32)
    (call $new)
    (array.len (struct.get $s 1 (array.get $a (local.get $i)))))

  (func (export "get_field2") (param $i i32) (result f64)
    (call $new)
    (struct.get $s 2 (array.get $a (local.get $i)))))`);


        assert.eq(m.exports.get_field0(0), 3);
        assert.eq(m.exports.get_field1_len(0), 1);
        var retVal = m.exports.get_field2(0);
        assert.eq(retVal >= 100 && retVal < 100.2, true);

        assert.eq(m.exports.get_field0(1), 42);
        assert.eq(m.exports.get_field1_len(1), 6);
        assert.eq(m.exports.get_field2(1), 0);

        assert.eq(m.exports.get_field0(2), 17);
        assert.eq(m.exports.get_field1_len(2), 7);
        assert.eq(m.exports.get_field2(2), -1.25);

        assert.eq(m.exports.get_field0(3), 5);
        assert.eq(m.exports.get_field1_len(3), 2);
        var retVal = m.exports.get_field2(3);
        assert.eq(retVal >= 3.14 && retVal < 3.15, true);

}

function testArrayNewFixed() {
    let m = instantiate(`(module
  (type $vec (array f32))
  (type $mvec (array (mut f32)))

  (func $new (result (ref $vec))
    (array.new_fixed $vec 2 (f32.const 1) (f32.const 2))
  )

  (func $get (param $i i32) (param $v (ref $vec)) (result f32)
    (array.get $vec (local.get $v) (local.get $i))
  )
  (func (export "get") (param $i i32) (result f32)
    (call $get (local.get $i) (call $new))
  )

  (func $set_get (param $i i32) (param $v (ref $mvec)) (param $y f32) (result f32)
    (array.set $mvec (local.get $v) (local.get $i) (local.get $y))
    (array.get $mvec (local.get $v) (local.get $i))
  )
  (func (export "set_get") (param $i i32) (param $y f32) (result f32)
    (call $set_get (local.get $i)
      (array.new_fixed $mvec 3 (f32.const 1) (f32.const 2) (f32.const 3))
      (local.get $y)
    )
  )

  (func $len (param $v (ref array)) (result i32)
    (array.len (local.get $v))
  )
  (func (export "len") (result i32)
    (call $len (call $new))
  )
)`);

    assert.eq(m.exports.get(0), 1.0);
    assert.eq(m.exports.set_get(1, 7.0), 7.0);
    assert.eq(m.exports.len(), 2);
    assert.throws(() => m.exports.get(10), WebAssembly.RuntimeError, "Out of bounds array.get");
    assert.throws(() => m.exports.set_get(10, 7.0), WebAssembly.RuntimeError, "Out of bounds array.set");
}

function testArrayNewFixedNested() {
    let m = instantiate(`(module
  (type $bvec (array (mut i8)))
  (type $mvec (array (mut (ref $bvec))))

  (func $new (result (ref $mvec))
    (array.new_fixed $mvec 2 (array.new_default $bvec (i32.const 5))
                             (array.new_default $bvec (i32.const 3)))
  )

  (func $set_get_array (param $i i32) (param $v (ref $mvec)) (param $element (ref $bvec)) (result i32)
    (array.set $mvec (local.get $v) (local.get $i) (local.get $element))
    (array.len (array.get $mvec (local.get $v) (local.get $i)))
  )

  (func $set_get_byte (param $i i32) (param $v (ref $mvec)) (param $j i32) (param $value i32) (result i32)
    (array.set $bvec (array.get $mvec (local.get $v) (local.get $i)) (local.get $j) (local.get $value))
    (array.get_u $bvec (array.get $mvec (local.get $v) (local.get $i)) (local.get $j))
  )

  (func (export "set_get_array") (param $i i32) (param $len i32) (result i32)
    (call $set_get_array (local.get $i)
      (call $new)
      (array.new_default $bvec (local.get $len)))
  )

  (func (export "set_get_byte") (param $i i32) (param $j i32) (param $value i32) (result i32)
    (call $set_get_byte (local.get $i)
      (call $new)
      (local.get $j)
      (local.get $value))
  )

  (func $len (param $v (ref array)) (result i32)
    (array.len (local.get $v))
  )
  (func (export "len") (result i32)
    (call $len (call $new))
  ))`);

    for (var i = 0; i < 2; i++) {
        for (var j = 0; j < (i === 0 ? 5 : 3); j++) {
            assert.eq(m.exports.set_get_byte(i, j, 127), 127);
        }
    }

    for (var i = 0; i < 2; i++) {
        assert.eq(m.exports.set_get_array(i, 12), 12);
    }

    assert.eq(m.exports.len(), 2);
    assert.throws(() => m.exports.set_get_array(10, 1), WebAssembly.RuntimeError, "Out of bounds array.set");
    assert.throws(() => m.exports.set_get_byte(10, 1, 42), WebAssembly.RuntimeError, "Out of bounds array.get");
    assert.throws(() => m.exports.set_get_byte(1, 4, 42), WebAssembly.RuntimeError, "Out of bounds array.set");
    assert.throws(() => m.exports.set_get_byte(0, 6, 42), WebAssembly.RuntimeError, "Out of bounds array.set");
}


function testArrayNewFixedNestedCalls() {
    let m = instantiate(`(module
  (type $bvec (array (mut i8)))
  (type $mvec (array (mut (ref $bvec))))

  (func $new (result (ref $mvec))
    (array.new_fixed $mvec 2 (array.new_fixed $bvec 3 (i32.const 1) (i32.const 42) (i32.const 5))
                             (array.new_fixed $bvec 4 (i32.const 2) (i32.const 23) (i32.const 10) (i32.const 1))))

  (func $get_byte (param $i i32) (param $v (ref $mvec)) (param $j i32) (result i32)
    (array.get_u $bvec (array.get $mvec (local.get $v) (local.get $i)) (local.get $j)))

  (func (export "get_byte") (param $i i32) (param $j i32) (result i32)
    (call $get_byte (local.get $i)
      (call $new)
      (local.get $j)))

  (func $len (param $v (ref array)) (result i32)
    (array.len (local.get $v)))

  (func (export "len") (result i32)
    (call $len (call $new)))

  (func (export "len_inner") (param $i i32) (result i32)
    (call $len (array.get $mvec (call $new) (local.get $i)))))`);

    assert.eq(m.exports.get_byte(0, 0), 1);
    assert.eq(m.exports.get_byte(0, 1), 42);
    assert.eq(m.exports.get_byte(0, 2), 5);
    assert.eq(m.exports.get_byte(1, 0), 2);
    assert.eq(m.exports.get_byte(1, 1), 23);
    assert.eq(m.exports.get_byte(1, 2), 10);
    assert.eq(m.exports.get_byte(1, 3), 1);
}

function testMissingArgumentCount() {
    /*
(module
  (type $vec (array f32))

  (func $new (export "new") (result (ref $vec))
    (array.new_fixed $vec)
  ))
     */

    assert.throws(() => new WebAssembly.Instance(module("\x00\x61\x73\x6D\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x5E\x7D\x00\x60\x00\x01\x64\x00\x03\x82\x80\x80\x80\x00\x01\x01\x07\x87\x80\x80\x80\x00\x01\x03\x6E\x65\x77\x00\x00\x0A\x91\x80\x80\x80\x00\x01\x8A\x80\x80\x80\x00\x00\x43\x00\x00\x80\x3F\xFB\x08\x00\x0B")), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 56: parsing ended before the end of Code section");

    /*
(module
  (type $vec (array f32))

  (func $new (export "new") (result (ref $vec))
    (array.new_fixed $vec <invalid i32>)
  ))
*/
    assert.throws(() => new WebAssembly.Instance(module("\x00\x61\x73\x6D\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x5E\x7D\x00\x60\x00\x01\x64\x00\x03\x82\x80\x80\x80\x00\x01\x01\x07\x87\x80\x80\x80\x00\x01\x03\x6E\x65\x77\x00\x00\x0A\x91\x80\x80\x80\x00\x01\x8B\x80\x80\x80\x00\x00\x43\x00\x00\x80\x3F\xFB\x08\x00\x99\x99")), WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 11: can't get argument count for array.new_fixed, in function at index 0");
}

function testTooManyOperands() {
    let moduleString = `(module
  (type $vec (array i32))

  (func $new (export "new") (result (ref $vec))
    (array.new_fixed $vec 10001)
  ))`;

    assert.throws(() => compile(moduleString), WebAssembly.CompileError, "WebAssembly.Module doesn't validate: array_new_fixed can take at most 10000 operands. Got 10001, in function at index 0");
}

testArrayNewFixed();
testArrayNewFixedNested();
testArrayNewFixedNestedCalls();
testArrayNewFixedImmutable();
testZeroLengthArray();
testSingletonArray()
testTypeMismatch();
testArgLengthMismatch();
testInvalidArrayType();
testSubtyping();

testPackedTypes();
testNumericTypes();
testNestedArrays();
testStructs();
testArrayFuncrefs();
testArrayExternrefs();
testArrayRefNull();
testMissingArgumentCount();
testTooManyOperands();
