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

function check(mString, expectedVal) {
    let m = instantiate(mString);
    let retVal = m.exports.f();
    assert.eq(BigInt(retVal), expectedVal);
}

function checkDataSegmentIndex(createModuleString, dataSegmentIndex, expectedVal) {
    let m = instantiate(createModuleString(dataSegmentIndex));
    let retVal = m.exports.f();
    assert.eq(retVal, expectedVal);
}

function testArrayNewCanonElem() {
    let arraySize = 3;
    let elemSegmentLength = 4;
    let f = (i) => `
      (module
        (func $f (result i32) (i32.const 37))
        (elem $elem0 funcref (ref.func $f) (ref.func $f) (ref.func $f) (ref.func $f))
        (type $a (array (mut funcref)))
        (func (export "f") (result funcref)
          (i32.const 0)
          (i32.const ` + arraySize + `)
          (array.new_canon_elem $a 0)
          (i32.const ` + i + `)
          (array.get $a)))`;

    for (var i = 0; i < arraySize; i++) {
        let m = instantiate(f(i));
        let retVal = m.exports.f();
        assert.eq(retVal(), 37);
    }
}

function testArrayNewCanonElemExternref() {
    let arraySize = 3;
    let elemSegmentLength = 4;
    let f = (i) => `
      (module
        (elem $elem0 externref (ref.null extern) (ref.null extern) (ref.null extern) (ref.null extern))
        (type $a (array (mut externref)))
        (func (export "f") (result externref)
          (i32.const 0)
          (i32.const ` + arraySize + `)
          (array.new_canon_elem $a 0)
          (i32.const ` + i + `)
          (array.get $a)))`;

    for (var i = 0; i < arraySize; i++) {
        let m = instantiate(f(i));
        let retVal = m.exports.f();
        assert.eq(retVal, null);
    }
}

function testBadTypeIndex() {
    let f = () => instantiate(`
      (module
        (func $f (result i32) (i32.const 37))
        (elem $elem0 funcref (ref.func $f) (ref.func $f) (ref.func $f) (ref.func $f))
        (type (array (mut funcref)))
        (func (export "f") (result funcref)
          (i32.const 0)
          (i32.const 2)
          (array.new_canon_elem 1 0)
          (i32.const 0)
          (array.get 0)))`);

    assert.throws(f, WebAssembly.CompileError, "WebAssembly.Module doesn't validate: array.new_elem type index 1 does not reference an array definition, in function at index 1");
}

function testNonArrayType() {
    let f = () => instantiate(`
      (module
        (func $f (result i32) (i32.const 37))
        (elem $elem0 funcref (ref.func $f) (ref.func $f) (ref.func $f) (ref.func $f))
        (type (array (mut funcref)))
        (type (func (result i32)))
        (func (export "f") (result funcref)
          (i32.const 0)
          (i32.const 2)
          (array.new_canon_elem 1 0)
          (i32.const 0)
          (array.get 0)))`);

    assert.throws(f, WebAssembly.CompileError, "WebAssembly.Module doesn't validate: array.new_elem type index 1 does not reference an array definition, in function at index 1");
}

function testImmutableArrayType() {
    let m = instantiate(`
      (module
        (func $f (result i32) (i32.const 37))
        (elem $elem0 funcref (ref.func $f) (ref.func $f) (ref.func $f) (ref.func $f))
        (type (array funcref))
        (func (export "f") (result funcref)
          (i32.const 0)
          (i32.const 2)
          (array.new_canon_elem 0 0)
          (i32.const 0)
          (array.get 0)))`);

    assert.eq(m.exports.f()(), 37);
}

function testWrongTypeOffset() {
    let f = () => instantiate(`
      (module
        (func $f (result i32) (i32.const 37))
        (elem $elem0 funcref (ref.func $f) (ref.func $f) (ref.func $f) (ref.func $f))
        (type (array (mut funcref)))
        (func (export "f") (result funcref)
          (f32.const 0)
          (i32.const 2)
          (array.new_canon_elem 0 0)
          (i32.const 0)
          (array.get 0)))`);

    assert.throws(f, WebAssembly.CompileError, "WebAssembly.Module doesn't validate: array.new_elem: offset has type F32 expected I32, in function at index 1");
}

function testWrongTypeSize() {
    let f = () => instantiate(`
      (module
        (func $f (result i32) (i32.const 37))
        (elem $elem0 funcref (ref.func $f) (ref.func $f) (ref.func $f) (ref.func $f))
        (type (array (mut funcref)))
        (func (export "f") (result funcref)
          (i32.const 0)
          (f32.const 2)
          (array.new_canon_elem 0 0)
          (i32.const 0)
          (array.get 0)))`);

    assert.throws(f, WebAssembly.CompileError, "WebAssembly.Module doesn't validate: array.new_elem: size has type F32 expected I32, in function at index 1");
}

function testNoElementSegments() {
    let f = () => instantiate(`
      (module
        (func $f (result i32) (i32.const 37))
        (type (array (mut funcref)))
        (func (export "f") (result funcref)
          (f32.const 0)
          (i32.const 2)
          (array.new_canon_elem 0 0)
          (i32.const 0)
          (array.get 0)))`);

    assert.throws(f, WebAssembly.CompileError, "WebAssembly.Module doesn't validate: array.new_elem in module with no elements segments, in function at index 1");
}

function testOutOfBoundsElementSegmentIndex() {
    let f = () => instantiate(`
      (module
        (func $f (result i32) (i32.const 37))
        (func $g (result i32) (i32.const 42))
        (elem $elem0 funcref (ref.func $f) (ref.func $f) (ref.func $f) (ref.func $f))
        (elem $elem1 funcref (ref.func $f) (ref.func $g) (ref.func $g) (ref.func $g))
        (type (array (mut funcref)))
        (func (export "f") (result funcref)
          (f32.const 0)
          (i32.const 2)
          (array.new_canon_elem 0 3)
          (i32.const 0)
          (array.get 0)))`);

    assert.throws(f, WebAssembly.CompileError, "WebAssembly.Module doesn't validate: array.new_elem segment index 3 is out of bounds (maximum element segment index is 1), in function at index 2");
}

function testTypeMismatch() {
    let f = () => instantiate(`
      (module
        (func $f (result i32) (i32.const 37))
        (elem $elem0 funcref (ref.func $f) (ref.func $f) (ref.func $f) (ref.func $f))
        (type $a1 (array (mut funcref)))
        (type $a2 (array (mut i32)))
        (func (export "f") (result funcref)
          (i32.const 0)
          (i32.const 2)
          (array.new_canon_elem $a2 0)
          (i32.const 0)
          (array.get 0)))`);

    assert.throws(f, WebAssembly.CompileError, "WebAssembly.Module doesn't validate: type mismatch in array.new_elem: segment elements have type funcref but array.new_elem operation expects elements of type I32, in function at index 1");
}

function testWrongNumberOfArguments() {
    // 0 arguments
    var f = () => instantiate(`
      (module
        (func $f (result i32) (i32.const 37))
        (elem $elem0 funcref (ref.func $f) (ref.func $f) (ref.func $f) (ref.func $f))
        (type $a1 (array (mut funcref)))
        (func (export "f") (result funcref)
          (array.new_canon_elem $a1 0)
          (i32.const 0)
          (array.get 0)))`);

    assert.throws(f, WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 5: can't pop empty stack in array.new_elem, in function at index 1");

    // 1 argument
    f = () => instantiate(`
      (module
        (func $f (result i32) (i32.const 37))
        (elem $elem0 funcref (ref.func $f) (ref.func $f) (ref.func $f) (ref.func $f))
        (type $a1 (array (mut funcref)))
        (func (export "f") (result funcref)
          (i32.const 0)
          (array.new_canon_elem $a1 0)
          (i32.const 0)
          (array.get 0)))`);

    assert.throws(f, WebAssembly.CompileError, "WebAssembly.Module doesn't parse at byte 7: can't pop empty stack in array.new_elem, in function at index 1");

    // 3 arguments
    f = () => instantiate(`
      (module
        (func $f (result i32) (i32.const 37))
        (elem $elem0 funcref (ref.func $f) (ref.func $f) (ref.func $f) (ref.func $f))
        (type $a1 (array (mut funcref)))
        (func (export "f") (result funcref)
          (i32.const 0)
          (i32.const 0)
          (i32.const 0)
          (array.new_canon_elem $a1 0)
          (i32.const 0)
          (array.get 0)))`);

    assert.throws(f, WebAssembly.CompileError, "WebAssembly.Module doesn't validate:  block with type: () -> [RefNull] returns: 1 but stack has: 2 values, in function at index 1");
}

//- offset + arraySize overflows int32
function testInt32Overflow() {
    var f = (offset, len) => instantiate(`
      (module
        (func $f (result i32) (i32.const 37))
        (elem $elem0 funcref (ref.func $f) (ref.func $f) (ref.func $f) (ref.func $f))
        (type $a (array (mut funcref)))
        (func (export "f") (result i32)
          (i32.const ` + offset + `)
          (i32.const ` + len + `)
          (array.new_canon_elem 0 0)
          (array.len)))`).exports.f();
    let maxUint32 = 0xffffffff;
    assert.throws(() => instantiate(f(0, maxUint32)),
                  WebAssembly.RuntimeError, "Offset + array length would exceed the length of an element segment");
    assert.throws(() => instantiate(f(1, maxUint32)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the length of an element segment");
    assert.throws(() => instantiate(f(2, maxUint32)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the length of an element segment");
    assert.throws(() => instantiate(f(2, maxUint32 - 1)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the length of an element segment");
    assert.throws(() => instantiate(f(10, maxUint32)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the length of an element segment");
    assert.throws(() => instantiate(f(maxUint32, maxUint32)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the length of an element segment");
    assert.throws(() => instantiate(f(maxUint32 - 4, 5)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the length of an element segment");
    assert.throws(() => instantiate(f(maxUint32, 1)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the length of an element segment");
    assert.throws(() => instantiate(f(maxUint32 - 1, 2)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the length of an element segment");

}

// Creating a zero-length array should work, as long as the element segment offset is valid
function testZeroLengthArray() {
    let f = (elements, offset) => (`
      (module
        (func $f (result i32) (i32.const 37))
        (elem $elem0 funcref ` + elements + `)
        (type $a (array (mut funcref)))
        (func (export "f") (result i32)
          (i32.const ` + offset + `)
          (i32.const 0)
          (array.new_canon_elem 0 0)
          (array.len)))`);
    // zero-length array from zero-length element segment; zero offset
    var m = f("", 0);
    assert.eq(instantiate(m).exports.f(), 0);
    // zero-length array from non-zero-length element segment; zero offset
    m = f("(ref.func $f) (ref.func $f)", 0);
    assert.eq(instantiate(m).exports.f(), 0);
    // zero-length array from zero-length element segment; non-zero offset; should throw
    m = f("", 3);
    assert.throws(() => instantiate(m).exports.f(),
                  WebAssembly.RuntimeError, "Offset + array length would exceed the length of an element segment");
    // zero-length array from non-zero-length data segment; non-zero offset
    m = f("(ref.func $f) (ref.func $f)", 1);
    assert.eq(instantiate(m).exports.f(), 0);
    // zero-length array from non-zero-length element segment; offset == length; this should succeed
    // ("traps if offset + size > len($e)")
    m = f("(ref.func $f) (ref.func $f) (ref.func $f)", 3);
    assert.eq(instantiate(m).exports.f(), 0);
    // zero-length array from non-zero-length element segment; offset > length; should throw
    m = f("(ref.func $f) (ref.func $f) (ref.func $f)", 4);
    assert.throws(() => instantiate(m).exports.f(), WebAssembly.RuntimeError, "Offset + array length would exceed the length of an element segment");
}

function testArrayNewCanonElemSubtype() {
    // Test array,new_canon_elem $t $e where $e: rt and rt <: $t
    let arraySize = 2;
    let m = instantiate(`
      (module
        (func $f (result i32) (i32.const 37))
        (type $fty (func (result i32)))
        (type $aty (array (mut funcref)))
        (elem $e (ref $fty) (ref.func $f) (ref.func $f))
        (func (export "f") (param $i i32) (result funcref)
          (i32.const 0)
          (i32.const 2)
          (array.new_canon_elem $aty 0)
          (local.get $i)
          (array.get $aty)))`);

    for (var i = 0; i < arraySize; i++) {
        let retVal = m.exports.f(i);
        assert.eq(retVal(), 37);
    }
}

function testRefCallNullary() {
  let m = instantiate(`(module
      (type $fty (func (result i32)))
      (type $a (array (mut (ref $fty))))
      (func $f (result i32)
         (i32.const 2))
      (func $g (result i32)
         (i32.const 3))
      (elem $e (ref $fty) (ref.func $f) (ref.func $g))
      (func (export "test") (param $i i32) (result i32)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (local.get $i)
         (array.get $a)
         (call_ref $fty)))`);

    var retVal = m.exports.test(0);
    assert.eq(retVal, 2);
    retVal = m.exports.test(1);
    assert.eq(retVal, 3);
}

function testRefCall() {
  let m = instantiate(`
    (module
      (type $fty (func (param i32) (result i32)))
      (type $a (array (mut (ref $fty))))

      (func $f (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 2)))
      (func $g (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 3)))

      (elem $e (ref $fty) (ref.func $f) (ref.func $g))
      (func (export "test") (param $i i32) (result i32)
         (i32.const 42)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (local.get $i)
         (array.get $a)
         (call_ref $fty)))`);

    var retVal = m.exports.test(0);
    assert.eq(retVal, 44);
    retVal = m.exports.test(1);
    assert.eq(retVal, 45);
}

function testIndirectCallNullary() {
  let m = instantiate(`(module
      (type $fty (func (result i32)))
      (type $a (array funcref))
      (func $f (result i32)
         (i32.const 2))
      (func $g (result i32)
         (i32.const 3))
      (table $t 1 funcref)
      (elem $e funcref (ref.func $f) (ref.func $g))
      (func (export "test") (param $i i32) (result i32)
         (i32.const 0)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (local.get $i)
         (array.get $a)
         (table.set $t)
         (i32.const 0)
         (call_indirect $t (type $fty))))`);

    var retVal = m.exports.test(0);
    assert.eq(retVal, 2);
    retVal = m.exports.test(1);
    assert.eq(retVal, 3);
}

function testIndirectCall() {
  let m = instantiate(`
    (module
      (type $fty (func (param i32) (result i32)))
      (type $a (array funcref))

      (func $f (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 2)))
      (func $g (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 3)))
      (table $t 1 funcref)
      (elem $e funcref (ref.func $f) (ref.func $g))
      (func (export "test") (param $i i32) (result i32)
         (table.set $t (i32.const 0)
           (array.get $a (array.new_canon_elem $a 0 (i32.const 0) (i32.const 2))
                         (local.get $i)))
         (call_indirect $t (type $fty) (i32.const 42) (i32.const 0))))`);

    var retVal = m.exports.test(0);
    assert.eq(retVal, 44);
    retVal = m.exports.test(1);
    assert.eq(retVal, 45);
}


function testAllElementSegmentKinds() {
    let check = (m) => {
        var retVal = m.exports.test(0);
        assert.eq(retVal, 44);
        retVal = m.exports.test(1);
        assert.eq(retVal, 45);
    }

    // elem ::= 0 e:expr y*:vec(funcidx)
    var m = instantiate(`
    (module
      (type $a (array (mut funcref)))

      (func $f (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 2)))
      (func $g (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 3)))

      (table $t 10 funcref)
      (elem $e (offset (i32.const 4)) $f $g $f $g)

      (func (export "test") (param $i i32) (result funcref)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (local.get $i)
         (array.get $a)))`);
    var returnedFun = m.exports.test(0);
    assert.eq(returnedFun(42), 44);
    returnedFun = m.exports.test(1);
    assert.eq(returnedFun(42), 45);

    // elem ::= 1 et:elemkind y*:vec(funcidx)
    m = instantiate(`
    (module
      (type $a (array (mut funcref)))
      (func $f (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 2)))
      (func $g (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 3)))
      (elem $e func $f $g $f $g)
      (func (export "test") (param $i i32) (result funcref)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (local.get $i)
         (array.get $a)))`);
    returnedFun = m.exports.test(0);
    assert.eq(returnedFun(42), 44);
    returnedFun = m.exports.test(1);
    assert.eq(returnedFun(42), 45);

    // elem ::= 2 x:tableidx e:expr et:elemkind y*:vec(funcidx)
    m = instantiate(`
    (module
      (type $a (array (mut funcref)))
      (func $f (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 2)))
      (func $g (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 3)))
      (table $t 10 funcref)
      (elem $e (table $t) (offset (i32.const 4)) func $f $g $f $g)
      (func (export "test") (param $i i32) (result funcref)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (local.get $i)
         (array.get $a)))`);
    returnedFun = m.exports.test(0);
    assert.eq(returnedFun(42), 44);
    returnedFun = m.exports.test(1);
    assert.eq(returnedFun(42), 45);

    // elem ::= 3 et:elemkind y*:vec(funcidx)
    m = instantiate(`
    (module
      (type $a (array (mut funcref)))
      (func $f (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 2)))
      (func $g (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 3)))
      (table $t 10 funcref)
      (elem declare func $f $g $f $g)
      (func (export "test") (param $i i32) (result funcref)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (local.get $i)
         (array.get $a)))`);
    returnedFun = m.exports.test(0);
    assert.eq(returnedFun(42), 44);
    returnedFun = m.exports.test(1);
    assert.eq(returnedFun(42), 45);

    // elem ::= 4 e:expr el*:vec(expr)
    m = instantiate(`
    (module
      (type $a (array (mut funcref)))
      (func $f (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 2)))
      (func $g (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 3)))
      (table $t 10 funcref)
      (elem (offset (i32.const 2)) $f $g $f $g)
      (func (export "test") (param $i i32) (result funcref)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (local.get $i)
         (array.get $a)))`);
    returnedFun = m.exports.test(0);
    assert.eq(returnedFun(42), 44);
    returnedFun = m.exports.test(1);
    assert.eq(returnedFun(42), 45);

    // elem ::= 5 et:reftype el*:vec(expr)

    // FIXME: Replace m with the following once https://bugs.webkit.org/show_bug.cgi?id=251874 is fixed
    /*
    m = instantiate(`
    (module
      (type $fty (func (param i32) (result i32)))
      (type $a (array (mut (ref $fty))))
      (func $f (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 2)))
      (func $g (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 3)))
      (elem $e (ref $fty) (ref.func $f) (ref.func $g))
      (func (export "test") (param $i i32) (result i32)
         (i32.const 42)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (local.get $i)
         (array.get $a)
         (call_ref $fty)))`);
    var retVal = m.exports.test(0);
    assert.eq(retVal, 44);
    retVal = m.exports.test(1);
    assert.eq(retVal, 45);
*/

        m = instantiate(`
    (module
      (type $fty (func (param i32) (result i32)))
      (type $a (array (mut funcref)))
      (table $t 1 funcref)
      (func $f (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 2)))
      (func $g (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 3)))
      (elem $e funcref (ref.func $f) (ref.func $g))
      (func (export "test") (param $i i32) (result i32)
         (i32.const 0)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (array.get $a (local.get $i))
         (table.set $t)
         (call_indirect $t (type $fty) (i32.const 42) (i32.const 0))))`);
    var retVal = m.exports.test(0);
    assert.eq(retVal, 44);
    retVal = m.exports.test(1);
    assert.eq(retVal, 45);

    // elem ::= 6 x:tableidx e:expr et:reftype el*:vec(expr) et:reftype el*:vec(expr)
    // FIXME: Replace m with the following once https://bugs.webkit.org/show_bug.cgi?id=251874 is fixed
    /*
    m = instantiate(`
    (module
      (type $fty (func (param i32) (result i32)))
      (type $a (array (mut (ref $fty))))
      (func $f (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 2)))
      (func $g (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 3)))
      (table $t 10 funcref)
      (elem $e (table $t) (offset (i32.const 4)) (ref $fty) (ref.func $f) (ref.func $g))
      (func (export "test") (param $i i32) (result i32)
         (i32.const 42)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (local.get $i)
         (array.get $a)
         (call_ref $fty)))`);
    */
    m = instantiate(`
    (module
      (type $fty (func (param i32) (result i32)))
      (type $a (array (mut funcref)))
      (func $f (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 2)))
      (func $g (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 3)))
      (table $t 10 funcref)
      (table $t1 1 funcref)
      (elem $e (table $t) (offset (i32.const 4)) funcref (ref.func $f) (ref.func $g))
      (func (export "test") (param $i i32) (result i32)
         (i32.const 0)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (array.get $a (local.get $i))
         (table.set $t1)
         (call_indirect $t1 (type $fty) (i32.const 42) (i32.const 0))))`);

    var retVal = m.exports.test(0);
    assert.eq(retVal, 44);
    retVal = m.exports.test(1);
    assert.eq(retVal, 45);

    // elem ::= 7 et:reftype el*:vec(expr)
    // FIXME: Replace m with the following once https://bugs.webkit.org/show_bug.cgi?id=251874 is fixed
/*
    m = instantiate(`
    (module
      (type $fty (func (param i32) (result i32)))
      (type $a (array (mut (ref $fty))))
      (func $f (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 2)))
      (func $g (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 3)))
      (table $t 10 funcref)
      (elem $e (ref $fty) (ref.func $f) (ref.func $g))
      (func (export "test") (param $i i32) (result i32)
         (i32.const 42)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (local.get $i)
         (array.get $a)
         (call_ref $fty)))`);
*/
    m = instantiate(`
    (module
      (type $fty (func (param i32) (result i32)))
      (type $a (array (mut funcref)))
      (func $f (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 2)))
      (func $g (param $i i32) (result i32)
         (i32.add (local.get $i) (i32.const 3)))
      (table $t 10 funcref)
      (elem $e funcref (ref.func $f) (ref.func $g))
      (func (export "test") (param $i i32) (result i32)
         (i32.const 0)
         (i32.const 0)
         (i32.const 2)
         (array.new_canon_elem $a 0)
         (array.get $a (local.get $i))
         (table.set $t)
         (call_indirect $t (type $fty) (i32.const 42) (i32.const 0))))`);
    var retVal = m.exports.test(0);
    assert.eq(retVal, 44);
    retVal = m.exports.test(1);
    assert.eq(retVal, 45);

}


function testNullFunctionIndex() {
    var m = instantiate(`
      (module
        (func $f)
        (func $g)
        (type (array (mut funcref)))
        (elem funcref (ref.func $f) (item ref.func $f) (item (ref.null func)) (ref.func $g))
        (func (export "f") (result funcref)
          (i32.const 0)
          (i32.const 4)
          (array.new_canon_elem 0 0)
          (i32.const 2)
          (array.get 0)))`);
    var retVal = m.exports.f();
    assert.eq(retVal, null);

    // If the element segment type is declared as non-null, then this should be rejected
    m = (`(module
        (func $f)
        (func $g)
        (type (array (mut (ref func))))
        (elem funcref (ref.func $f) (item ref.func $f) (item (ref.null func)) (ref.func $g))
        (func (export "f") (result funcref)
          (i32.const 0)
          (i32.const 4)
          (array.new_canon_elem 0 0)
          (i32.const 2)
          (array.get 0)))`);
    assert.throws(() => compile(m),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't validate: type mismatch in array.new_elem: segment elements have type funcref but array.new_elem operation expects elements of type Ref, in function at index 2");

    // Element segment type is a subtype of declared array type: this should work
    m = instantiate(`(module
        (func $f)
        (func $g)
        (type (array (mut funcref)))
        (elem (ref func) (ref.func $f) (item ref.func $f) (ref.func $g) (ref.func $g))
        (func (export "f") (result funcref)
          (i32.const 0)
          (i32.const 4)
          (array.new_canon_elem 0 0)
          (i32.const 2)
          (array.get 0)))`);
    var retVal = m.exports.f();
    assert.eq(typeof retVal, "function");
}

function testImportFunctions() {
    // Test imported wasm functions
    let m1 = instantiate(`
      (module
        (func (export "func"))
        (type $t0 (func (param i32) (result)))
        (type $t1 (func (param f32) (result)))
        (type $t2 (func (param f64) (result)))
        (func (export "func_i32") (type $t0) (param i32) (result) (nop))
        (func (export "func_f32") (type $t1) (param f32) (result) (nop))
        (func (export "func_f64") (type $t2) (param f64) (result) (nop)))`);

    let imports = { m: {} };
    imports.m["func_i32"] = m1.exports["func_i32"];
    imports.m["func_f32"] = m1.exports["func_f32"];
    imports.m["func_f64"] = m1.exports["func_f64"];

    let m = instantiate(`
      (module
        (type $a (array (mut funcref)))
        (import "m" "func_i32" (func $func_i32 (param i32) (result)))
        (import "m" "func_f32" (func $func_f32 (param f32) (result)))
        (import "m" "func_f64" (func $func_f64 (param f64) (result)))

      (elem funcref (ref.func $func_i32) (ref.func $func_f32) (ref.func $func_f64))
      (func (export "f") (result funcref)
        (i32.const 0)
        (i32.const 3)
        (array.new_canon_elem 0 0)
        (i32.const 2)
        (array.get $a)))`, imports);

    var retVal = m.exports.f();
    assert.eq(typeof retVal, "function");
}

function testJSFunctions() {
    // Test imported JS functions

    let imports = { };
    imports["./imports.js"] = {};
    imports["./imports.js"]["func_i32"] = function(x) { return (x + 1) };
    imports["./imports.js"]["func_f32"] = function(x) { return (x + 2.4) };
    imports["./imports.js"]["func_f64"] = function(x) { return (x + 3.5) };

    let m = instantiate(`
      (module
        (type $a (array (mut funcref)))
        (import "./imports.js" "func_i32" (func $func_i32 (param i32) (result)))
        (import "./imports.js" "func_f32" (func $func_f32 (param f32) (result)))
        (import "./imports.js" "func_f64" (func $func_f64 (param f64) (result)))

        (elem funcref (ref.func $func_i32) (ref.func $func_f32) (ref.func $func_f64))
        (func (export "f") (result funcref)
          (i32.const 0)
          (i32.const 3)
          (array.new_canon_elem 0 0)
          (i32.const 0)
          (array.get $a))
        (func (export "g") (result funcref)
          (i32.const 0)
          (i32.const 3)
          (array.new_canon_elem 0 0)
          (i32.const 1)
          (array.get $a))
        (func (export "h") (result funcref)
          (i32.const 0)
          (i32.const 3)
          (array.new_canon_elem 0 0)
          (i32.const 2)
          (array.get $a)))`, imports);

    var retVal = m.exports.f()(42);
    assert.eq(retVal, 43);
    retVal = m.exports.g()(43);
    assert.eq(retVal, 45.4);
    retVal = m.exports.h()(44.0);
    assert.eq(retVal, 47.5);
}

// FIXME: uncomment these tests once https://bugs.webkit.org/show_bug.cgi?id=251874 is fixed
// testRefCallNullary();
// testRefCall();
// testArrayNewCanonElemExternref();
// testArrayNewCanonElemSubtype();

testArrayNewCanonElem();
testIndirectCallNullary();
testIndirectCall();
testAllElementSegmentKinds();
testBadTypeIndex();
testNonArrayType();
testImmutableArrayType();
testWrongTypeOffset();
testWrongTypeSize();
testNoElementSegments();
testOutOfBoundsElementSegmentIndex();
testTypeMismatch();
testWrongNumberOfArguments();
testInt32Overflow();
testZeroLengthArray();
testNullFunctionIndex();
testImportFunctions();
testJSFunctions();
