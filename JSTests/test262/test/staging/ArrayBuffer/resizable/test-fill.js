// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from TestFill test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
includes: [compareArray.js]
features: [resizable-arraybuffer]
flags: [onlyStrict]
---*/

class MyUint8Array extends Uint8Array {
}

class MyFloat32Array extends Float32Array {
}

class MyBigInt64Array extends BigInt64Array {
}

const builtinCtors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Float32Array,
  Float64Array,
  Uint8ClampedArray,
  BigUint64Array,
  BigInt64Array
];

const ctors = [
  ...builtinCtors,
  MyUint8Array,
  MyFloat32Array,
  MyBigInt64Array
];

function CreateResizableArrayBuffer(byteLength, maxByteLength) {
  return new ArrayBuffer(byteLength, { maxByteLength: maxByteLength });
}

function ReadDataFromBuffer(ab, ctor) {
  let result = [];
  const ta = new ctor(ab, 0, ab.byteLength / ctor.BYTES_PER_ELEMENT);
  for (let item of ta) {
    result.push(Number(item));
  }
  return result;
}

function TypedArrayFillHelper(ta, n, start, end) {
  if (ta instanceof BigInt64Array || ta instanceof BigUint64Array) {
    ta.fill(BigInt(n), start, end);
  } else {
    ta.fill(n, start, end);
  }
}

function ArrayFillHelper(ta, n, start, end) {
  if (ta instanceof BigInt64Array || ta instanceof BigUint64Array) {
    Array.prototype.fill.call(ta, BigInt(n), start, end);
  } else {
    Array.prototype.fill.call(ta, n, start, end);
  }
}

function TestFill(helper, oobThrows) {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      0,
      0,
      0,
      0
    ]);
    helper(fixedLength, 1);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      1,
      1,
      1,
      1
    ]);
    helper(fixedLengthWithOffset, 2);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      1,
      1,
      2,
      2
    ]);
    helper(lengthTracking, 3);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      3,
      3,
      3,
      3
    ]);
    helper(lengthTrackingWithOffset, 4);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      3,
      3,
      4,
      4
    ]);

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);
    if (oobThrows) {
      assert.throws(TypeError, () => helper(fixedLength, 5));
      assert.throws(TypeError, () => helper(fixedLengthWithOffset, 6));
    } else {
      helper(fixedLength, 5);
      helper(fixedLengthWithOffset, 6);
      // We'll check below that these were no-op.
    }
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      3,
      3,
      4
    ]);
    helper(lengthTracking, 7);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      7,
      7,
      7
    ]);
    helper(lengthTrackingWithOffset, 8);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      7,
      7,
      8
    ]);

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);
    if (oobThrows) {
      assert.throws(TypeError, () => helper(fixedLength, 9));
      assert.throws(TypeError, () => helper(fixedLengthWithOffset, 10));
      assert.throws(TypeError, () => helper(lengthTrackingWithOffset, 11));
    } else {
      // We'll check below that these were no-op.
      helper(fixedLength, 9);
      helper(fixedLengthWithOffset, 10);
      helper(lengthTrackingWithOffset, 11);
    }
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [7]);
    helper(lengthTracking, 12);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [12]);

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
    helper(fixedLength, 13);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      13,
      13,
      13,
      13,
      0,
      0
    ]);
    helper(fixedLengthWithOffset, 14);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      13,
      13,
      14,
      14,
      0,
      0
    ]);
    helper(lengthTracking, 15);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      15,
      15,
      15,
      15,
      15,
      15
    ]);
    helper(lengthTrackingWithOffset, 16);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      15,
      15,
      16,
      16,
      16,
      16
    ]);

    // Filling with non-undefined start & end.
    helper(fixedLength, 17, 1, 3);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      15,
      17,
      17,
      16,
      16,
      16
    ]);
    helper(fixedLengthWithOffset, 18, 1, 2);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      15,
      17,
      17,
      18,
      16,
      16
    ]);
    helper(lengthTracking, 19, 1, 3);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      15,
      19,
      19,
      18,
      16,
      16
    ]);
    helper(lengthTrackingWithOffset, 20, 1, 2);
    assert.compareArray(ReadDataFromBuffer(rab, ctor), [
      15,
      19,
      19,
      20,
      16,
      16
    ]);
  }
}

TestFill(TypedArrayFillHelper, true);
TestFill(ArrayFillHelper, false);
