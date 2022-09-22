// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from ForEachReduceReduceRight test
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

const TypedArrayForEachHelper = (ta, ...rest) => {
  return ta.forEach(...rest);
};

const ArrayForEachHelper = (ta, ...rest) => {
  return Array.prototype.forEach.call(ta, ...rest);
};

const TypedArrayReduceHelper = (ta, ...rest) => {
  return ta.reduce(...rest);
};

const ArrayReduceHelper = (ta, ...rest) => {
  return Array.prototype.reduce.call(ta, ...rest);
};

const TypedArrayReduceRightHelper = (ta, ...rest) => {
  return ta.reduceRight(...rest);
};

const ArrayReduceRightHelper = (ta, ...rest) => {
  return Array.prototype.reduceRight.call(ta, ...rest);
};

function ForEachReduceReduceRight(forEachHelper, reduceHelper, reduceRightHelper, oobThrows) {
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

    function Helper(array) {
      const forEachValues = [];
      const reduceValues = [];
      const reduceRightValues = [];
      forEachHelper(array, n => {
        forEachValues.push(n);
      });
      reduceHelper(array, (acc, n) => {
        reduceValues.push(n);
      }, 'initial value');
      reduceRightHelper(array, (acc, n) => {
        reduceRightValues.push(n);
      }, 'initial value');
      assert.compareArray(forEachValues, reduceValues);
      reduceRightValues.reverse();
      assert.compareArray(reduceRightValues, reduceValues);
      return ToNumbers(forEachValues);
    }
    assert.compareArray(Helper(fixedLength), [
      0,
      2,
      4,
      6
    ]);
    assert.compareArray(Helper(fixedLengthWithOffset), [
      4,
      6
    ]);
    assert.compareArray(Helper(lengthTracking), [
      0,
      2,
      4,
      6
    ]);
    assert.compareArray(Helper(lengthTrackingWithOffset), [
      4,
      6
    ]);

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    if (oobThrows) {
      assert.throws(TypeError, () => {
        Helper(fixedLength);
      });
      assert.throws(TypeError, () => {
        Helper(fixedLengthWithOffset);
      });
    } else {
      assert.compareArray(Helper(fixedLength), []);
      assert.compareArray(Helper(fixedLengthWithOffset), []);
    }
    assert.compareArray(Helper(lengthTracking), [
      0,
      2,
      4
    ]);
    assert.compareArray(Helper(lengthTrackingWithOffset), [4]);

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        Helper(fixedLength);
      });
      assert.throws(TypeError, () => {
        Helper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        Helper(lengthTrackingWithOffset);
      });
    } else {
      assert.compareArray(Helper(fixedLength), []);
      assert.compareArray(Helper(fixedLengthWithOffset), []);
    }
    assert.compareArray(Helper(lengthTracking), [0]);

    // Shrink to zero.
    rab.resize(0);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        Helper(fixedLength);
      });
      assert.throws(TypeError, () => {
        Helper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        Helper(lengthTrackingWithOffset);
      });
    } else {
      assert.compareArray(Helper(fixedLength), []);
      assert.compareArray(Helper(fixedLengthWithOffset), []);
      assert.compareArray(Helper(lengthTrackingWithOffset), []);
    }
    assert.compareArray(Helper(lengthTracking), []);

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

    assert.compareArray(Helper(fixedLength), [
      0,
      2,
      4,
      6
    ]);
    assert.compareArray(Helper(fixedLengthWithOffset), [
      4,
      6
    ]);
    assert.compareArray(Helper(lengthTracking), [
      0,
      2,
      4,
      6,
      8,
      10
    ]);
    assert.compareArray(Helper(lengthTrackingWithOffset), [
      4,
      6,
      8,
      10
    ]);
  }
}

ForEachReduceReduceRight(TypedArrayForEachHelper, TypedArrayReduceHelper, TypedArrayReduceRightHelper, true);
ForEachReduceReduceRight(ArrayForEachHelper, ArrayReduceHelper, ArrayReduceRightHelper, false);
