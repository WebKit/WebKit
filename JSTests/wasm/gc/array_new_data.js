//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
    for (let i = 0; i < bytes.length; ++i) {
        print(bytes.charCodeAt(i));
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

function check(createModuleString, type, getter, resultType, expectedVal) {
    let mString = createModuleString(type, getter, resultType);
    let m = instantiate(mString);
    let retVal = m.exports.f();
    assert.eq(BigInt(retVal), expectedVal);
}

function assertFails(m, msg) {
    assert.throws(() => instantiate(m).exports.f(),
                  WebAssembly.RuntimeError, msg)
}

function checkDataSegmentIndex(createModuleString, dataSegmentIndex, expectedVal) {
    let m = instantiate(createModuleString(dataSegmentIndex));
    let retVal = m.exports.f();
    assert.eq(retVal, expectedVal);
}

// Test array.new_data with all numeric and packed types
function testArrayNewCanonData() {
    let arraySize = 8;
    let string = "WebAssemblyGarbageCollectionProposalWebAssemblyGarbageCollectionProposal";
    // The following creates a 36-byte passive data segment, then initializes a 16-element array from that segment
    // (reading 16 * sizeof(t) bytes for type t),
    // starting at offset 0 of the data segment
    let f = (i) => (type, getter, resultType) => `
      (module
        (memory (export "memory") 1)
        (data "` + string + `")
        (type (array ` + type + `))
        (func (export "f") (result ` + resultType + `)
          (i32.const 0)
          (i32.const ` + arraySize + `)
          (array.new_data 0 0)
          (i32.const ` + i + `)
          (array.` + getter + ` 0)))
    `;

    // To test arrays of type i`n`, loop through the contents of
    // the data section in n-byte chunks, interpreting each chunk
    // as an i`n` and comparing the result of the array.get against
    // this value
    function testArray(type, getter, resultType, size) {
        var stringIndex = 0;
        for (var i = 0; i < arraySize; i++) {
            var expectedVal = 0n;
            for (var j = size - 1; j >= 0; j--) {
                let byte = string[stringIndex + j].charCodeAt(0);
                expectedVal += BigInt(byte * 2**(j*8));
            }
            check(f(i), type, getter, resultType, expectedVal);
            stringIndex += size;
        }
    }

    testArray("(mut i8)", "get_u", "i32", 1);
    testArray("(mut i16)", "get_u", "i32", 2);
    testArray("(mut i32)", "get", "i32", 4);
    testArray("(mut i64)", "get", "i64", 8);
    testArray("i8", "get_u", "i32", 1);
    testArray("i16", "get_u", "i32", 2);
    testArray("i32", "get", "i32", 4);
    testArray("i64", "get", "i64", 8);

}

// A data segment outside the range of declared data
// segments should be a compile-time error
function testBadDataSegment() {
    assert.throws(() => compile(`
      (module
        (memory (export "memory") 1)
        (data "foo")
        (type (array (mut i8)))
        (func (export "f") (result i32)
          (i32.const 0)
          (i32.const 1)
          (array.new_data 0 2)
          (i32.const 0)
          (array.get_u 0)))`),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't validate: array.new_data segment index 2 is out of bounds (maximum data segment index is 0)");
    // Test module with no data segments
    assert.throws(() => compile(`
      (module
        (type (array (mut i8)))
        (func (export "f") (result i32)
          (i32.const 0)
          (i32.const 1)
          (array.new_data 0 0)
          (i32.const 0)
          (array.get_u 0)))`),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't validate: array.new_data in module with no data segments, in function at index 0");

}

function testOtherDataSegments() {
    let f = (dataSegmentIndex) => (`
      (module
        (memory (export "memory") 1)
        (data "snails")
        (data "thanks")
        (data "snthanks")
        (data "ggggggggggg")
        (data "999999999999")
        (type (array (mut i8)))
        (func (export "f") (result i32)
          (i32.const 0)
          (i32.const 1)
          (array.new_data 0 ` + dataSegmentIndex + `)
          (i32.const 0)
          (array.get_u 0)))`);

    for (const i of [[0, 's'], [1, 't'], [2, 's'], [3, 'g'], [4, '9']]) {
        checkDataSegmentIndex(f, i[0], i[1].charCodeAt(0));
    }
}

// An offset greater than the length of a data segment
// should be a runtime error
function testBadOffset() {
    function check(m) {
        m.exports.f();
    }
    assert.throws(() => check(instantiate(`
      (module
        (memory (export "memory") 1)
        (data "foo")
        (type (array (mut i8)))
        (func (export "f") (result i32)
          (i32.const 10)
          (i32.const 2)
          (array.new_data 0 0)
          (i32.const 0)
          (array.get_u 0)))`)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the size of a data segment");
    assert.throws(() => check(instantiate(`
      (module
        (memory (export "memory") 1)
        (data "foo")
        (type (array (mut i8)))
        (func (export "f") (result i32)
          (i32.const -2)
          (i32.const 1)
          (array.new_data 0 0)
          (i32.const 0)
          (array.get_u 0)))`)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the size of a data segment");
}

function testOffsets() {
    let string = "abcdefghijklmnopqrstuvwxyz";
    let checkOneOffset = (offset) => { (dataSegmentIndex) => {
        let m = (`
          (module
            (memory (export "memory") 1)
               (data "` + string + `")
               (type (array (mut i8)))
               (func (export "f") (result i32)
                 (i32.const ` + offset + `)
                 (i32.const 2)
                 (array.new_data 0 ` + dataSegmentIndex + `)
                 (i32.const 0)
                 (array.get_u 0)))`);
        var i = 0;
        for (c of string) {
           checkDataSegmentIndex(checkOneOffset, 0, string.charCodeAt(i));
           i++;
        }
    }}
    for (var offset = 0; offset < (string.length - 1); offset++) {
        checkOneOffset(offset);
    }

}

// If `offset` is within bounds but `offset` + (array length in bytes) is
// out of bounds, that should also be a runtime error
function testReadOutOfBounds() {
    let string = "abcdefghij";
    let check  = (typeBits, offset, arrayLength) => {
      let suffix = typeBits <= 16 ? "_u" : "";
      let resultBits = typeBits < 64 ? 32 : 64;
      assert.throws(() => (instantiate(`
        (module
          (memory (export "memory") 1)
          (data "` + string + `")
          (type (array (mut i` + typeBits + `)))
          (func (export "f") (result i` + resultBits + `)
            (i32.const ` + offset + `)
            (i32.const ` + arrayLength + `)
            (array.new_data 0 0)
            (i32.const 0)
            (array.get` + suffix + ` 0)))`)).exports.f(),
                  WebAssembly.RuntimeError,
                    "Offset + array length would exceed the size of a data segment")
    };
    // Test {i8, i16, i32, i64}
    for (const type of [8, 16, 32, 64]) {
        // Test every in-bounds offset
        for (var offset = 0; offset < string.length; offset++) {
            let elementSize = type/8; // in bytes
            // Number of possible elements of `type`'s size in the portion of the data segment starting at `offset`
            let dataSegmentElements = Math.floor((string.length - offset) / elementSize);
            for (const extra of [1, 2, 3, 4]) {
                // Add bytes so that the read of the data segment would be out of bounds
                let arrayLength = dataSegmentElements + extra;
                check(type, offset, arrayLength);
            }
        }
    }
}

function testInt32Overflow() {
    var f = (offset, len) => instantiate(`
      (module
        (memory (export "memory") 1)
        (data "aaaaaaa")
        (type (array (mut i8)))
        (func (export "f") (result i32)
          (i32.const ` + offset + `)
          (i32.const ` + len + `)
          (array.new_data 0 0)
          (array.len)))`).exports.f();
    let maxUint32 = 0xffffffff;
    assert.throws(() => instantiate(f(1, maxUint32)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the size of a data segment");
    assert.throws(() => instantiate(f(2, maxUint32)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the size of a data segment");
    assert.throws(() => instantiate(f(2, maxUint32 - 1)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the size of a data segment");
    assert.throws(() => instantiate(f(10, maxUint32)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the size of a data segment");
    assert.throws(() => instantiate(f(maxUint32, maxUint32)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the size of a data segment");
    assert.throws(() => instantiate(f(maxUint32 - 4, 5)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the size of a data segment");
    assert.throws(() => instantiate(f(maxUint32, 1)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the size of a data segment");
    assert.throws(() => instantiate(f(maxUint32 - 1, 2)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the size of a data segment");
    // Check for overflow when multiplying element size by array size
    f = (offset, len) => instantiate(`
      (module
        (memory (export "memory") 1)
        (data "aaaaaaa")
        (type (array (mut i32)))
        (func (export "f") (result i32)
          (i32.const ` + offset + `)
          (i32.const ` + len + `)
          (array.new_data 0 0)
          (array.len)))`).exports.f();
    // (maxUint32 / 4) + 1
    let badArraySize = 0x40000000;
    assert.throws(() => instantiate(f(0, badArraySize)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the size of a data segment");
    assert.throws(() => instantiate(f(1, badArraySize - 1)),
                  WebAssembly.RuntimeError,
                  "Offset + array length would exceed the size of a data segment");
}


// Creating a zero-length array should work, as long as the data segment offset is valid
function testZeroLengthArray() {
    // zero-length array from zero-length data segment; zero offset
    let f = (string, offset) => (`
      (module
        (memory (export "memory") 1)
        (data "` + string + `")
        (type (array (mut i8)))
        (func (export "f") (result i32)
          (i32.const ` + offset + `)
          (i32.const 0)
          (array.new_data 0 0)
          (array.len)))`);
    var m = f("", 0);
    assert.eq(instantiate(m).exports.f(), 0);
    // zero-length array from non-zero data segment; zero offset
    m = f("xyz", 0);
    assert.eq(instantiate(m).exports.f(), 0);
    // zero-length array from zero-length data segment; non-zero offset; should throw
    m = f("", 3);
    assert.throws(() => instantiate(m).exports.f(),
                  WebAssembly.RuntimeError, "Offset + array length would exceed the size of a data segment");
    // zero-length array from non-zero-length data segment; non-zero offset
    m = f("xyz", 1);
    assert.eq(instantiate(m).exports.f(), 0);
    // zero-length array from non-zero-length data segment; offset == length; this should succeed
    // ("traps if offset + |t'|*size > len($d)")
    m = f("xyz", 3);
    assert.eq(instantiate(m).exports.f(), 0);
    // zero-length array from non-zero-length data segment; offset > length; should throw
    m = f("xyz", 4);
    assert.throws(() => instantiate(m).exports.f(), WebAssembly.RuntimeError, "Offset + array length would exceed the size of a data segment");
}

function testSingletonArray() {
    let f = (string, offset) => (`
      (module
        (memory (export "memory") 1)
        (data "` + string + `")
        (type (array (mut i8)))
        (func (export "f") (result i32)
          (i32.const ` + offset + `)
          (i32.const 1)
          (array.new_data 0 0)
          (array.len)))`);
    let msg = "Offset + array length would exceed the size of a data segment";
    // singleton array from 0-length data segment -- should throw
    var m = f("", 0);
    assertFails(m, msg);
    // singleton array from 1-length data segment, 0 offset
    m = f("z", 0);
    assert.eq(instantiate(m).exports.f(), 1);
    // singleton array from 2-length data segment
    m = f("yz", 0);
    assert.eq(instantiate(m).exports.f(), 1);
    // singleton array from 1-length data segment, non-zero offset - should throw
    m = f("z", 1);
    assertFails(m, msg);
    // singleton array from 2-length data segment; offset = 1
    m = f("yz", 1);
    assert.eq(instantiate(m).exports.f(), 1);
    // singleton array from non-zero-length data segment; offset = 2; should throw
    m = f("yz", 2);
    assertFails(m, msg);
}

function testTypeErrors() {
    let notArray = "WebAssembly.Module doesn't validate: array.new_data index 0 does not reference an array definition, in function at index 0";
    let refType = "WebAssembly.Module doesn't validate: array.new_data expected numeric, packed, or vector type; found RefNull";
    let erroneousCase = (ty, msg) => {
        assert.throws(() => compile (`
          (module
            (memory (export "memory") 1)
               (data "")
               (type ` + ty + `)
               (func (export "f") (result i32)
                 (i32.const 0)
                 (i32.const 2)
                 (array.new_data 0 0)))`), WebAssembly.CompileError, msg);
    };
    /*
      `array.new_data $t $d` should be a type error if:
        * $t is non-array (struct or func)
        * $t is an array of t' where t' is not numeric, vector or packed
     */
    for (const [badType, message] of [["(struct (field i64) (field i32))", notArray],
                           ["(func (result i32))", notArray],
                           ["(array (mut funcref))", refType],
                           ["(array (mut externref))", refType]]) {
        erroneousCase(badType, message);
    }
}

function testBadOperands() {
    assert.throws(() => compile (`
          (module
            (memory (export "memory") 1)
               (data "")
               (type (array (mut i32)))
               (func (export "f") (result i32)
                 (i64.const 0)
                 (i32.const 2)
                 (array.new_data 0 0) (drop) (i32.const 0)))`),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't validate: array.new_data: offset has type I64 expected I32");
    assert.throws(() => compile (`
          (module
            (memory (export "memory") 1)
               (data "")
               (type (array (mut i32)))
               (func (export "f") (result i32)
                 (i32.const 0)
                 (i64.const 2)
                 (array.new_data 0 0) (drop) (i32.const 0)))`),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't validate: array.new_data: size has type I64 expected I32");
    assert.throws(() => compile (`
          (module
            (memory (export "memory") 1)
               (data "")
               (type (array (mut i32)))
               (func (export "f") (result i32)
                 (i32.const 0)
                 (array.new_data 0 0) (drop) (i32.const 0)))`),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't parse at byte 7: can't pop empty stack in array.new_data, in function at index 0");
    assert.throws(() => compile (`
          (module
            (memory (export "memory") 1)
               (data "")
               (type (array (mut i32)))
               (func (export "f") (result i32)
                 (array.new_data 0 0) (drop) (i32.const 0)))`),
                  WebAssembly.CompileError,
                  "WebAssembly.Module doesn't parse at byte 5: can't pop empty stack in array.new_data, in function at index 0");
}

function testRecGroup() {
    instantiate(`
        (module
          (memory (export "memory") 1)
          (data "snails")
          (rec (type (struct))
               (type $arr (array (mut i8))))
          (func (export "f") (result i32)
            (i32.const 0)
            (i32.const 1)
            (array.new_data $arr 0)
            (i32.const 0)
            (array.get_u $arr)))
    `);
}

testArrayNewCanonData();
testBadDataSegment();
testOtherDataSegments();
testBadOffset();
testOffsets();
testReadOutOfBounds();
testInt32Overflow();
testZeroLengthArray();
testSingletonArray();
testTypeErrors();
testBadOperands();
testRecGroup();
