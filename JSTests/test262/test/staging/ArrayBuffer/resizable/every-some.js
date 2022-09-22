// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from EverySome test
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

const TypedArrayEveryHelper = (ta, ...rest) => {
  return ta.every(...rest);
};

const ArrayEveryHelper = (ta, ...rest) => {
  return Array.prototype.every.call(ta, ...rest);
};

const TypedArraySomeHelper = (ta, ...rest) => {
  return ta.some(...rest);
};

const ArraySomeHelper = (ta, ...rest) => {
  return Array.prototype.some.call(ta, ...rest);
};

function EverySome(everyHelper, someHelper, oobThrows) {
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

    function div3(n) {
      return Number(n) % 3 == 0;
    }
    function even(n) {
      return Number(n) % 2 == 0;
    }
    function over10(n) {
      return Number(n) > 10;
    }
    assert(!everyHelper(fixedLength, div3));
    assert(everyHelper(fixedLength, even));
    assert(someHelper(fixedLength, div3));
    assert(!someHelper(fixedLength, over10));
    assert(!everyHelper(fixedLengthWithOffset, div3));
    assert(everyHelper(fixedLengthWithOffset, even));
    assert(someHelper(fixedLengthWithOffset, div3));
    assert(!someHelper(fixedLengthWithOffset, over10));
    assert(!everyHelper(lengthTracking, div3));
    assert(everyHelper(lengthTracking, even));
    assert(someHelper(lengthTracking, div3));
    assert(!someHelper(lengthTracking, over10));
    assert(!everyHelper(lengthTrackingWithOffset, div3));
    assert(everyHelper(lengthTrackingWithOffset, even));
    assert(someHelper(lengthTrackingWithOffset, div3));
    assert(!someHelper(lengthTrackingWithOffset, over10));

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    if (oobThrows) {
      assert.throws(TypeError, () => {
        everyHelper(fixedLength, div3);
      });
      assert.throws(TypeError, () => {
        someHelper(fixedLength, div3);
      });
      assert.throws(TypeError, () => {
        everyHelper(fixedLengthWithOffset, div3);
      });
      assert.throws(TypeError, () => {
        someHelper(fixedLengthWithOffset, div3);
      });
    } else {
      assert(everyHelper(fixedLength, div3));
      assert(!someHelper(fixedLength, div3));
      assert(everyHelper(fixedLengthWithOffset, div3));
      assert(!someHelper(fixedLengthWithOffset, div3));
    }
    assert(!everyHelper(lengthTracking, div3));
    assert(everyHelper(lengthTracking, even));
    assert(someHelper(lengthTracking, div3));
    assert(!someHelper(lengthTracking, over10));
    assert(!everyHelper(lengthTrackingWithOffset, div3));
    assert(everyHelper(lengthTrackingWithOffset, even));
    assert(!someHelper(lengthTrackingWithOffset, div3));
    assert(!someHelper(lengthTrackingWithOffset, over10));

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        everyHelper(fixedLength, div3);
      });
      assert.throws(TypeError, () => {
        someHelper(fixedLength, div3);
      });
      assert.throws(TypeError, () => {
        everyHelper(fixedLengthWithOffset, div3);
      });
      assert.throws(TypeError, () => {
        someHelper(fixedLengthWithOffset, div3);
      });
      assert.throws(TypeError, () => {
        everyHelper(lengthTrackingWithOffset, div3);
      });
      assert.throws(TypeError, () => {
        someHelper(lengthTrackingWithOffset, div3);
      });
    } else {
      assert(everyHelper(fixedLength, div3));
      assert(!someHelper(fixedLength, div3));
      assert(everyHelper(fixedLengthWithOffset, div3));
      assert(!someHelper(fixedLengthWithOffset, div3));
      assert(everyHelper(lengthTrackingWithOffset, div3));
      assert(!someHelper(lengthTrackingWithOffset, div3));
    }
    assert(everyHelper(lengthTracking, div3));
    assert(everyHelper(lengthTracking, even));
    assert(someHelper(lengthTracking, div3));
    assert(!someHelper(lengthTracking, over10));

    // Shrink to zero.
    rab.resize(0);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        everyHelper(fixedLength, div3);
      });
      assert.throws(TypeError, () => {
        someHelper(fixedLength, div3);
      });
      assert.throws(TypeError, () => {
        everyHelper(fixedLengthWithOffset, div3);
      });
      assert.throws(TypeError, () => {
        someHelper(fixedLengthWithOffset, div3);
      });
      assert.throws(TypeError, () => {
        everyHelper(lengthTrackingWithOffset, div3);
      });
      assert.throws(TypeError, () => {
        someHelper(lengthTrackingWithOffset, div3);
      });
    } else {
      assert(everyHelper(fixedLength, div3));
      assert(!someHelper(fixedLength, div3));
      assert(everyHelper(fixedLengthWithOffset, div3));
      assert(!someHelper(fixedLengthWithOffset, div3));
      assert(everyHelper(lengthTrackingWithOffset, div3));
      assert(!someHelper(lengthTrackingWithOffset, div3));
    }
    assert(everyHelper(lengthTracking, div3));
    assert(everyHelper(lengthTracking, even));
    assert(!someHelper(lengthTracking, div3));
    assert(!someHelper(lengthTracking, over10));

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

    assert(!everyHelper(fixedLength, div3));
    assert(everyHelper(fixedLength, even));
    assert(someHelper(fixedLength, div3));
    assert(!someHelper(fixedLength, over10));
    assert(!everyHelper(fixedLengthWithOffset, div3));
    assert(everyHelper(fixedLengthWithOffset, even));
    assert(someHelper(fixedLengthWithOffset, div3));
    assert(!someHelper(fixedLengthWithOffset, over10));
    assert(!everyHelper(lengthTracking, div3));
    assert(everyHelper(lengthTracking, even));
    assert(someHelper(lengthTracking, div3));
    assert(!someHelper(lengthTracking, over10));
    assert(!everyHelper(lengthTrackingWithOffset, div3));
    assert(everyHelper(lengthTrackingWithOffset, even));
    assert(someHelper(lengthTrackingWithOffset, div3));
    assert(!someHelper(lengthTrackingWithOffset, over10));
  }
}

EverySome(TypedArrayEveryHelper, TypedArraySomeHelper, true);
EverySome(ArrayEveryHelper, ArraySomeHelper, false);
