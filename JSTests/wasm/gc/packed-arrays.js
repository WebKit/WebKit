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

function check(createModuleString, type, expectedVal) {
    let m = instantiate(createModuleString(type));
    assert.eq(m.exports.f(), expectedVal);
}

function testArrayGetPacked() {
    let f = (type) => `
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon_default 0 (i32.const 5))
          (i32.const 2)
          (array.get_u 0)))
    `;
    check(f, "i8", 0);
    check(f, "i16", 0);
}

function testArrayGetUWithNewCanonPacked() {
    /*
     * maxint32 => truncate to 0xFF => zero-extend to 0x000000FF (i8) or 0x0000FFFF (i16)
     */
    {
        let f = (type) => `
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 0x7FFF_FFFF) (i32.const 5))
          (i32.const 2)
          (array.get_u 0)))
    `;
        check(f, "i8", 255);
        check(f, "i16", 0xFFFF);
    }

    /*
     * -1 => truncate to 0xFF => zero-extend to 0x000000FF (i8) or 0x0000FFFF (i16)
     */
    {
        let f = (type) => `
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 0xFFFF_FFFF) (i32.const 5))
          (i32.const 2)
          (array.get_u 0)))
    `;
        check(f, "i8", 255);
        check(f, "i16", 0xFFFF);
    }

    /*
     * 0x4000_0000 => truncate to 0 => zero-extend to 0
     */
    {
        let f = (type) => `
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 0x4000_0000) (i32.const 5))
          (i32.const 2)
          (array.get_u 0)))
    `;
        check(f, "i8", 0);
        check(f, "i16", 0);
    }

    /*
     * 0x00000080 => truncate to 0x80  => zero-extend to 128
     */
 {
     let f = (type) => `
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 0x80) (i32.const 5))
          (i32.const 2)
          (array.get_u 0)))
    `;
     check(f, "i8", 128);
     check(f, "i16", 128);
  }

    /*
     * 0xaaaa_aaaa => truncate to 0xaa  => zero-extend to 0xaa (i8) or 0xaaaa (i16)
     */
 {
     let f = (type) => `
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 0xaaaa_aaaa) (i32.const 5))
          (i32.const 2)
          (array.get_u 0)))
    `;
     check(f, "i8", 0xaa);
     check(f, "i16", 0xaaaa);
  }

}

// These tests test two different things at once (that array.get_s sign-extends properly,
// and that array.new_canon truncates properly)
function testArrayGetSWithNewCanonPacked() {
    /*
     * maxint32 => truncate to 0xFF (or 0xFFFF) => sign-extend to 0xFFFFFFFF
     */

    {
        let f = (type) => `
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 0x7FFF_FFFF) (i32.const 5))
          (i32.const 2)
          (array.get_s 0)))
    `;
        check(f, "i8", -1);
        check(f, "i16", -1);
    }

    /*
     * -1 => truncate to 0xFF (or 0xFFFF) => sign-extend to 0xFFFFFFFF
     */

    {
        let f = (type) => `
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 0xFFFF_FFFF) (i32.const 5))
          (i32.const 2)
          (array.get_s 0)))
    `;
        check(f, "i8", -1);
        check(f, "i16", -1);
    }

    /*
     * 0x4000_0000 => truncate to 0 => sign-extend to 0
     */

  {
      let f = (type) => `
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 0x4000_0000) (i32.const 5))
          (i32.const 2)
          (array.get_s 0)))
    `;
      check(f, "i8", 0);
      check(f, "i16", 0);
  }

    /*
     * 0x0000_0080 => truncate to 0x80 => sign-extend to -128 (i8) or 128 (i16)
     */

  {
      let f = (type) => `
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 0x0000_0080) (i32.const 5))
          (i32.const 2)
          (array.get_s 0)))
    `;
      check(f, "i8", -128);
      check(f, "i16", 128);
  }

    /*
     * 0x0000_8000 => truncate to 0 (i8) 0x8000 (i16) => sign-extend to 0 (i8) or 0xffff8000 (-32768) (i16)
     */

    {
      let f = (type) => `
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 0x0000_8000) (i32.const 5))
          (i32.const 2)
          (array.get_s 0)))
    `;
        check(f, "i8", 0);
        check(f, "i16", -32768);
    }


    /*
     * 0xaaaa_aaaa => truncate to 0xaa (i8) or 0xaaaa (i16) => sign-extend to 0xffff_ffaa (-86) (i8) or 0xffff_aaaa (-21846) (i16)
     */

    {
        let f = (type) => `
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 0xaaaa_aaaa) (i32.const 5))
          (i32.const 2)
          (array.get_s 0)))
    `;
        check(f, "i8", -86);
        check(f, "i16", -21846);
    }

}

// Should be an error to initialize a packed array with an i64 value
function testTypeMismatch64() {
    let check = (type) => assert.throws(
        () => compile(`
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon 0 (i64.const 0) (i32.const 5))
          (i32.const 2)
          (array.get_u 0)))
    `),
        WebAssembly.CompileError,
        "array.new value to type I64 expected I32"
    );
    check("i8");
    check("i16");
}


// Should be an error to use array.get on a packed array -- need get_s or get_u
function testTypeMismatchArrayGet() {
    let f = (type) => () => compile(`
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
          (array.new_canon 0 (i32.const 0) (i32.const 5))
          (i32.const 2)
          (array.get 0)))
    `);
    assert.throws(f("i8"),
                  WebAssembly.CompileError,
                  "array.get applied to packed array of I8 -- use array.get_s or array.get_u");
    assert.throws(f("i16"),
                  WebAssembly.CompileError,
                  "array.get applied to packed array of I16 -- use array.get_s or array.get_u");
}

/*
Should be an error to use i8 or i16 outside an array context
*/
function testPackedTypeOutOfContext() {
/*
(module
  (func (export "f") (result i8)
    (i32.const 2)))
*/
    assert.throws(() => new WebAssembly.Instance(module("\x00\x61\x73\x6D\x01\x00\x00\x00\x01\x85\x80\x80\x80\x00\x01\x60\x00\x01\x7A\x03\x82\x80\x80\x80\x00\x01\x00\x07\x85\x80\x80\x80\x00\x01\x01\x66\x00\x00\x0A\x8A\x80\x80\x80\x00\x01\x84\x80\x80\x80\x00\x00\x41\x02\x0B")),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't parse at byte 19: can't get 0th Type's return value");
/*
(module
  (func (export "f") (param i16) (result i32)
    (i32.const 2)))
*/
    assert.throws(() => new WebAssembly.Instance(module("\x00\x61\x73\x6D\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x79\x01\x7F\x03\x82\x80\x80\x80\x00\x01\x00\x07\x85\x80\x80\x80\x00\x01\x01\x66\x00\x00\x0A\x8A\x80\x80\x80\x00\x01\x84\x80\x80\x80\x00\x00\x41\x02\x0B")),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't parse at byte 18: can't get 0th argument Type");
/*
(module
  (type (func (param i8) (result i32))))
*/
    assert.throws(() => new WebAssembly.Instance(module("\x00\x61\x73\x6D\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x7A\x01\x7F")),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't parse at byte 18: can't get 0th argument Type");
/*
(module
  (type (func (param i32) (result i16))))
*/
    assert.throws(() => new WebAssembly.Instance(module("\x00\x61\x73\x6D\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x7F\x01\x79")),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't parse at byte 20: can't get 0th Type's return value");
/*
(module
  (global (mut i8) (i32.const 0)))
*/
    assert.throws(() => new WebAssembly.Instance(module("\x00\x61\x73\x6D\x01\x00\x00\x00\x06\x86\x80\x80\x80\x00\x01\x7A\x01\x41\x00\x0B")),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't parse at byte 16: can't get Global's value type");
/*
(module
  (global (mut i16) (i32.const 0)))
*/
    assert.throws(() => new WebAssembly.Instance(module("\x00\x61\x73\x6D\x01\x00\x00\x00\x06\x86\x80\x80\x80\x00\x01\x79\x01\x41\x00\x0B")),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't parse at byte 16: can't get Global's value type");
}

// Tests that setting `element` in an array of `type` is a type error; also takes `elementType`
// so the right error message can be tested for
function testTypeError(type, element, elementType) {
    assert.throws(() => compile(`
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
        (array.new_canon_default 0 (i32.const 5))
        (i32.const 2)`
        + element +
        `\n(array.set 0)
        (array.get_u 0)))`),
                  WebAssembly.CompileError,
                  "array.set value to type " + elementType + " expected I32");
}

// Tests that setting an element in a null array of type `type` is a runtime error
function testTrapsNull(type) {
    let m = instantiate(`
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
        (ref.null 0)
        (i32.const 2)
        (i32.const 17)
        (array.set 0)
        (i32.const 0)))`);
    assert.throws(() => { m.exports.f(); },
                  WebAssembly.RuntimeError,
                  "array.set to a null reference");
}

// Tests that setting `index` in a `type` array of length `len` is a runtime error
function testTrapsOutOfBounds(type, len, index) {
    let m = instantiate(`
      (module
        (type (array (mut ` + type + `)))
        (func (export "f") (result i32)
        (array.new_canon_default 0 (i32.const ` + len + `))
        (i32.const ` + index + `)
        (i32.const 17)
        (array.set 0)
        (i32.const 0)))`);
    assert.throws(() => { m.exports.f(); },
                  WebAssembly.RuntimeError,
                  "Out of bounds array.set");
}

// Tests that if we set a[`index`] to (i32.const `value`), where a is a `type` array of length `len`, the following get operation returns (i32.const `value`)
function testSetGetRoundtrip(type, len, index, value) {
    let m = instantiate(`
      (module
        (type (array (mut ` + type + `)))
        (global (mut (ref null 0)) (ref.null 0))
        (func (export "init")
           (global.set 0 (array.new_canon_default 0 (i32.const ` + len + `))))
        (func (export "f") (result i32)
          (array.set 0 (global.get 0) (i32.const ` + index + `) (i32.const ` + value + `))
          (array.get_u 0 (global.get 0) (i32.const ` + index + `))))`);
    m.exports.init();
    assert.eq(m.exports.f(), value);
}

// Creates an array a of type `type`, sets a[i] (for some in-bounds i) to `inValue`, and gets `a[i]` using `signMode`; compares the result to `outValue`
function testSetGetTruncate(type, signMode, inValue, outValue) {
    let m = instantiate(`
      (module
        (type (array (mut ` + type + `)))
        (global (mut (ref null 0)) (ref.null 0))
        (func (export "init")
           (global.set 0 (array.new_canon_default 0 (i32.const 5))))
        (func (export "f") (result i32)
          (array.set 0 (global.get 0) (i32.const 3) (i32.const ` + inValue + `))
          (array.get_` + signMode + ` 0 (global.get 0) (i32.const 3))))`);
    m.exports.init();
    assert.eq(m.exports.f(), outValue);
}

/*
  array.set
  for both i8 and i16:

  test that it's a type error to pass in a non-i32 value when setting a packed array element
  test that it traps correctly for null
    and if the index is out of bounds
  test array lengths 0, 1, 5
  test indices 0, 1, and (for length 5) 3 and 4

  test that set/get roundtrips for within-bounds values
  test that set/get returns the properly truncated value for each combination of type and sign-extension
 */

function testArraySet() {
    for (const type of ["i8", "i16"]) {
        testTypeError(type, "(i64.const 3)", "I64");
        testTrapsNull(type);
        testTrapsOutOfBounds(type, 5, 5);
        testTrapsOutOfBounds(type, 5, 7);
        testTrapsOutOfBounds(type, 0, 1);
        for (const length of [0, 1, 5]) {
            for (const index of [0, 1, 3, 4]) {
                if (index < length) {
                    testSetGetRoundtrip(type, length, index, 17);
                } else {
                    testTrapsOutOfBounds(type, length, index);
                }
            }
        }
    }
    // testSetGetTruncate(type, signMode, inValue, outValue)
    testSetGetTruncate("i8", "u", 0xFFFF_FFFF, 255);
    testSetGetTruncate("i8", "u", 0x4000_0000, 0);
    testSetGetTruncate("i8", "u", 0x80, 128);
    testSetGetTruncate("i8", "s", 0xFFFF_FFFF, -1);
    testSetGetTruncate("i8", "s", 0x4000_0000, 0);
    testSetGetTruncate("i8", "s", 0x80, -128);
    testSetGetTruncate("i16", "u", 0xFFFF_FFFF, 0xFFFF);
    testSetGetTruncate("i16", "u", 0x4000_0000, 0);
    testSetGetTruncate("i16", "u", 0x80, 128);
    testSetGetTruncate("i16", "s", 0xFFFF_FFFF, -1);
    testSetGetTruncate("i16", "s", 0x4000_0000, 0);
    testSetGetTruncate("i16", "s", 0x80, 128);
    testSetGetTruncate("i16", "s", 0x7FFF_FFFF, -1);
    testSetGetTruncate("i16", "s", 0x4000_0000, 0);
    testSetGetTruncate("i16", "s", 0x80, 128);
    testSetGetTruncate("i16", "s", 0x8000, -32768);
    testSetGetTruncate("i16", "s", 0xaaaa_aaaa, -21846);
}

function testArrayGetUnreachable() {
/*
(module
   (type (array (mut i8)))
   (func (export "f") (result i32)
      (return)
      (array.new_canon_default 0 (i32.const 5))
      (i32.const 2)
      (array.get_s -1)))
*/
    assert.throws(() => new WebAssembly.Instance(module("\x00\x61\x73\x6D\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x5E\x7A\x01\x60\x00\x01\x7F\x03\x82\x80\x80\x80\x00\x01\x01\x07\x85\x80\x80\x80\x00\x01\x01\x66\x00\x00\x0A\x95\x80\x80\x80\x00\x01\x8F\x80\x80\x80\x00\x00\x41\x2A\x0F\x41\x05\xFB\x12\x00\x41\x02\xFB\x14\xFF\xFF")),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't parse at byte 15: can't get type index immediate for array.get_s in unreachable context");
/*
(module
   (type (array (mut i8)))
   (func (export "f") (result i32)
      (return)
      (array.new_canon_default 0 (i32.const 5))
      (i32.const 2)
      (array.get_u -1)))
*/
    assert.throws(() => new WebAssembly.Instance(module("\x00\x61\x73\x6D\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x5E\x7A\x01\x60\x00\x01\x7F\x03\x82\x80\x80\x80\x00\x01\x01\x07\x85\x80\x80\x80\x00\x01\x01\x66\x00\x00\x0A\x95\x80\x80\x80\x00\x01\x8F\x80\x80\x80\x00\x00\x41\x2A\x0F\x41\x05\xFB\x12\x00\x41\x02\xFB\x15\xFF\xFF")),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't parse at byte 15: can't get type index immediate for array.get_u in unreachable context");
}

testArrayGetPacked();
testArrayGetUWithNewCanonPacked();
testArrayGetSWithNewCanonPacked();
testTypeMismatch64();
testTypeMismatchArrayGet();
testPackedTypeOutOfContext();
testArraySet();
testArrayGetUnreachable();
