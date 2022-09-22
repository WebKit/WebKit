// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from TestCopyWithin test
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

function WriteToTypedArray(array, index, value) {
  if (array instanceof BigInt64Array || array instanceof BigUint64Array) {
    array[index] = BigInt(value);
  } else {
    array[index] = value;
  }
}

function Convert(item) {
  if (typeof item == 'bigint') {
    return Number(item);
  }
  return item;
}

function ToNumbers(array) {
  let result = [];
  for (let item of array) {
    result.push(Convert(item));
  }
  return result;
}

const TypedArrayCopyWithinHelper = (ta, ...rest) => {
  ta.copyWithin(...rest);
};

const ArrayCopyWithinHelper = (ta, ...rest) => {
  Array.prototype.copyWithin.call(ta, ...rest);
};

function TestCopyWithin(helper, oobThrows) {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    // Orig. array: [0, 1, 2, 3]
    //              [0, 1, 2, 3] << fixedLength
    //                    [2, 3] << fixedLengthWithOffset
    //              [0, 1, 2, 3, ...] << lengthTracking
    //                    [2, 3, ...] << lengthTrackingWithOffset

    helper(fixedLength, 0, 2);
    assert.compareArray(ToNumbers(fixedLength), [
      2,
      3,
      2,
      3
    ]);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }
    helper(fixedLengthWithOffset, 0, 1);
    assert.compareArray(ToNumbers(fixedLengthWithOffset), [
      3,
      3
    ]);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }
    helper(lengthTracking, 0, 2);
    assert.compareArray(ToNumbers(lengthTracking), [
      2,
      3,
      2,
      3
    ]);
    helper(lengthTrackingWithOffset, 0, 1);
    assert.compareArray(ToNumbers(lengthTrackingWithOffset), [
      3,
      3
    ]);

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 3; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    // Orig. array: [0, 1, 2]
    //              [0, 1, 2, ...] << lengthTracking
    //                    [2, ...] << lengthTrackingWithOffset

    if (oobThrows) {
      assert.throws(TypeError, () => {
        helper(fixedLength, 0, 1);
      });
      assert.throws(TypeError, () => {
        helper(fixedLengthWithOffset, 0, 1);
      });
    } else {
      helper(fixedLength, 0, 1);
      helper(fixedLengthWithOffset, 0, 1);
      // We'll check below that these were no-op.
    }
    assert.compareArray(ToNumbers(lengthTracking), [
      0,
      1,
      2
    ]);
    helper(lengthTracking, 0, 1);
    assert.compareArray(ToNumbers(lengthTracking), [
      1,
      2,
      2
    ]);
    helper(lengthTrackingWithOffset, 0, 1);
    assert.compareArray(ToNumbers(lengthTrackingWithOffset), [2]);

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);
    WriteToTypedArray(taWrite, 0, 0);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        helper(fixedLength, 0, 1, 1);
      });
      assert.throws(TypeError, () => {
        helper(fixedLengthWithOffset, 0, 1, 1);
      });
      assert.throws(TypeError, () => {
        helper(lengthTrackingWithOffset, 0, 1, 1);
      });
    } else {
      helper(fixedLength, 0, 1, 1);
      helper(fixedLengthWithOffset, 0, 1, 1);
      helper(lengthTrackingWithOffset, 0, 1, 1);
    }
    assert.compareArray(ToNumbers(lengthTracking), [0]);
    helper(lengthTracking, 0, 0, 1);
    assert.compareArray(ToNumbers(lengthTracking), [0]);

    // Shrink to zero.
    rab.resize(0);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        helper(fixedLength, 0, 1, 1);
      });
      assert.throws(TypeError, () => {
        helper(fixedLengthWithOffset, 0, 1, 1);
      });
      assert.throws(TypeError, () => {
        helper(lengthTrackingWithOffset, 0, 1, 1);
      });
    } else {
      helper(fixedLength, 0, 1, 1);
      helper(fixedLengthWithOffset, 0, 1, 1);
      helper(lengthTrackingWithOffset, 0, 1, 1);
    }
    assert.compareArray(ToNumbers(lengthTracking), []);
    helper(lengthTracking, 0, 0, 1);
    assert.compareArray(ToNumbers(lengthTracking), []);

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    // Orig. array: [0, 1, 2, 3, 4, 5]
    //              [0, 1, 2, 3] << fixedLength
    //                    [2, 3] << fixedLengthWithOffset
    //              [0, 1, 2, 3, 4, 5, ...] << lengthTracking
    //                    [2, 3, 4, 5, ...] << lengthTrackingWithOffset

    helper(fixedLength, 0, 2);
    assert.compareArray(ToNumbers(fixedLength), [
      2,
      3,
      2,
      3
    ]);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }
    helper(fixedLengthWithOffset, 0, 1);
    assert.compareArray(ToNumbers(fixedLengthWithOffset), [
      3,
      3
    ]);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    //              [0, 1, 2, 3, 4, 5, ...] << lengthTracking
    //        target ^     ^ start
    helper(lengthTracking, 0, 2);
    assert.compareArray(ToNumbers(lengthTracking), [
      2,
      3,
      4,
      5,
      4,
      5
    ]);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    //                    [2, 3, 4, 5, ...] << lengthTrackingWithOffset
    //              target ^  ^ start
    helper(lengthTrackingWithOffset, 0, 1);
    assert.compareArray(ToNumbers(lengthTrackingWithOffset), [
      3,
      4,
      5,
      5
    ]);
  }
}

TestCopyWithin(TypedArrayCopyWithinHelper, true);
TestCopyWithin(ArrayCopyWithinHelper, false);
