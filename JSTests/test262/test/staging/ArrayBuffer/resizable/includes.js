// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from Includes test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
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

function WriteToTypedArray(array, index, value) {
  if (array instanceof BigInt64Array || array instanceof BigUint64Array) {
    array[index] = BigInt(value);
  } else {
    array[index] = value;
  }
}

function TypedArrayIncludesHelper(array, n, fromIndex) {
  if (typeof n == 'number' && (array instanceof BigInt64Array || array instanceof BigUint64Array)) {
    return array.includes(BigInt(n), fromIndex);
  }
  return array.includes(n, fromIndex);
}

function ArrayIncludesHelper(array, n, fromIndex) {
  if (typeof n == 'number' && (array instanceof BigInt64Array || array instanceof BigUint64Array)) {
    return Array.prototype.includes.call(array, BigInt(n), fromIndex);
  }
  return Array.prototype.includes.call(array, n, fromIndex);
}

function Includes(helper, oobThrows) {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, ...] << lengthTracking
    //                    [4, 6, ...] << lengthTrackingWithOffset

    assert(helper(fixedLength, 2));
    assert(!helper(fixedLength, undefined));
    assert(helper(fixedLength, 2, 1));
    assert(!helper(fixedLength, 2, 2));
    assert(helper(fixedLength, 2, -3));
    assert(!helper(fixedLength, 2, -2));
    assert(!helper(fixedLengthWithOffset, 2));
    assert(helper(fixedLengthWithOffset, 4));
    assert(!helper(fixedLengthWithOffset, undefined));
    assert(helper(fixedLengthWithOffset, 4, 0));
    assert(!helper(fixedLengthWithOffset, 4, 1));
    assert(helper(fixedLengthWithOffset, 4, -2));
    assert(!helper(fixedLengthWithOffset, 4, -1));
    assert(helper(lengthTracking, 2));
    assert(!helper(lengthTracking, undefined));
    assert(helper(lengthTracking, 2, 1));
    assert(!helper(lengthTracking, 2, 2));
    assert(helper(lengthTracking, 2, -3));
    assert(!helper(lengthTracking, 2, -2));
    assert(!helper(lengthTrackingWithOffset, 2));
    assert(helper(lengthTrackingWithOffset, 4));
    assert(!helper(lengthTrackingWithOffset, undefined));
    assert(helper(lengthTrackingWithOffset, 4, 0));
    assert(!helper(lengthTrackingWithOffset, 4, 1));
    assert(helper(lengthTrackingWithOffset, 4, -2));
    assert(!helper(lengthTrackingWithOffset, 4, -1));

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    if (oobThrows) {
      assert.throws(TypeError, () => {
        helper(fixedLength, 2);
      });
      assert.throws(TypeError, () => {
        helper(fixedLengthWithOffset, 2);
      });
    } else {
      assert(!helper(fixedLength, 2));
      assert(!helper(fixedLengthWithOffset, 2));
    }
    assert(helper(lengthTracking, 2));
    assert(!helper(lengthTracking, undefined));
    assert(!helper(lengthTrackingWithOffset, 2));
    assert(helper(lengthTrackingWithOffset, 4));
    assert(!helper(lengthTrackingWithOffset, undefined));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        helper(fixedLength, 2);
      });
      assert.throws(TypeError, () => {
        helper(fixedLengthWithOffset, 2);
      });
      assert.throws(TypeError, () => {
        helper(lengthTrackingWithOffset, 2);
      });
    } else {
      assert(!helper(fixedLength, 2));
      assert(!helper(fixedLengthWithOffset, 2));
      assert(!helper(lengthTrackingWithOffset, 2));
    }

    // Shrink to zero.
    rab.resize(0);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        helper(fixedLength, 2);
      });
      assert.throws(TypeError, () => {
        helper(fixedLengthWithOffset, 2);
      });
      assert.throws(TypeError, () => {
        helper(lengthTrackingWithOffset, 2);
      });
    } else {
      assert(!helper(fixedLength, 2));
      assert(!helper(fixedLengthWithOffset, 2));
      assert(!helper(lengthTrackingWithOffset, 2));
    }
    assert(!helper(lengthTracking, 2));
    assert(!helper(lengthTracking, undefined));

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6, 8, 10]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, 8, 10, ...] << lengthTracking
    //                    [4, 6, 8, 10, ...] << lengthTrackingWithOffset

    assert(helper(fixedLength, 2));
    assert(!helper(fixedLength, undefined));
    assert(!helper(fixedLength, 8));
    assert(!helper(fixedLengthWithOffset, 2));
    assert(helper(fixedLengthWithOffset, 4));
    assert(!helper(fixedLengthWithOffset, undefined));
    assert(!helper(fixedLengthWithOffset, 8));
    assert(helper(lengthTracking, 2));
    assert(!helper(lengthTracking, undefined));
    assert(helper(lengthTracking, 8));
    assert(!helper(lengthTrackingWithOffset, 2));
    assert(helper(lengthTrackingWithOffset, 4));
    assert(!helper(lengthTrackingWithOffset, undefined));
    assert(helper(lengthTrackingWithOffset, 8));
  }
}

Includes(TypedArrayIncludesHelper, true);
Includes(ArrayIncludesHelper, false);
