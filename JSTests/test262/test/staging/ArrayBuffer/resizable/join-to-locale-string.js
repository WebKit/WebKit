// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from JoinToLocaleString test
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

const TypedArrayJoinHelper = (ta, ...rest) => {
  return ta.join(...rest);
};

const ArrayJoinHelper = (ta, ...rest) => {
  return Array.prototype.join.call(ta, ...rest);
};

const TypedArrayToLocaleStringHelper = (ta, ...rest) => {
  return ta.toLocaleString(...rest);
};

const ArrayToLocaleStringHelper = (ta, ...rest) => {
  return Array.prototype.toLocaleString.call(ta, ...rest);
};

function JoinToLocaleString(joinHelper, toLocaleStringHelper, oobThrows) {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    const taWrite = new ctor(rab);

    // Write some data into the array.
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, ...] << lengthTracking
    //                    [4, 6, ...] << lengthTrackingWithOffset

    assert.sameValue(joinHelper(fixedLength), '0,2,4,6');
    assert.sameValue(toLocaleStringHelper(fixedLength), '0,2,4,6');
    assert.sameValue(joinHelper(fixedLengthWithOffset), '4,6');
    assert.sameValue(toLocaleStringHelper(fixedLengthWithOffset), '4,6');
    assert.sameValue(joinHelper(lengthTracking), '0,2,4,6');
    assert.sameValue(toLocaleStringHelper(lengthTracking), '0,2,4,6');
    assert.sameValue(joinHelper(lengthTrackingWithOffset), '4,6');
    assert.sameValue(toLocaleStringHelper(lengthTrackingWithOffset), '4,6');

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    if (oobThrows) {
      assert.throws(TypeError, () => {
        joinHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        toLocaleStringHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        joinHelper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        toLocaleStringHelper(fixedLengthWithOffset);
      });
    } else {
      assert.sameValue(joinHelper(fixedLength), '');
      assert.sameValue(toLocaleStringHelper(fixedLength), '');
      assert.sameValue(joinHelper(fixedLengthWithOffset), '');
      assert.sameValue(toLocaleStringHelper(fixedLengthWithOffset), '');
    }
    assert.sameValue(joinHelper(lengthTracking), '0,2,4');
    assert.sameValue(toLocaleStringHelper(lengthTracking), '0,2,4');
    assert.sameValue(joinHelper(lengthTrackingWithOffset), '4');
    assert.sameValue(toLocaleStringHelper(lengthTrackingWithOffset), '4');

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        joinHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        toLocaleStringHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        joinHelper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        toLocaleStringHelper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        joinHelper(lengthTrackingWithOffset);
      });
      assert.throws(TypeError, () => {
        toLocaleStringHelper(lengthTrackingWithOffset);
      });
    } else {
      assert.sameValue(joinHelper(fixedLength), '');
      assert.sameValue(toLocaleStringHelper(fixedLength), '');
      assert.sameValue(joinHelper(fixedLengthWithOffset), '');
      assert.sameValue(toLocaleStringHelper(fixedLengthWithOffset), '');
      assert.sameValue(joinHelper(lengthTrackingWithOffset), '');
      assert.sameValue(toLocaleStringHelper(lengthTrackingWithOffset), '');
    }
    assert.sameValue(joinHelper(lengthTracking), '0');
    assert.sameValue(toLocaleStringHelper(lengthTracking), '0');

    // Shrink to zero.
    rab.resize(0);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        joinHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        toLocaleStringHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        joinHelper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        toLocaleStringHelper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        joinHelper(lengthTrackingWithOffset);
      });
      assert.throws(TypeError, () => {
        toLocaleStringHelper(lengthTrackingWithOffset);
      });
    } else {
      assert.sameValue(joinHelper(fixedLength), '');
      assert.sameValue(toLocaleStringHelper(fixedLength), '');
      assert.sameValue(joinHelper(fixedLengthWithOffset), '');
      assert.sameValue(toLocaleStringHelper(fixedLengthWithOffset), '');
      assert.sameValue(joinHelper(lengthTrackingWithOffset), '');
      assert.sameValue(toLocaleStringHelper(lengthTrackingWithOffset), '');
    }
    assert.sameValue(joinHelper(lengthTracking), '');
    assert.sameValue(toLocaleStringHelper(lengthTracking), '');

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

    assert.sameValue(joinHelper(fixedLength), '0,2,4,6');
    assert.sameValue(toLocaleStringHelper(fixedLength), '0,2,4,6');
    assert.sameValue(joinHelper(fixedLengthWithOffset), '4,6');
    assert.sameValue(toLocaleStringHelper(fixedLengthWithOffset), '4,6');
    assert.sameValue(joinHelper(lengthTracking), '0,2,4,6,8,10');
    assert.sameValue(toLocaleStringHelper(lengthTracking), '0,2,4,6,8,10');
    assert.sameValue(joinHelper(lengthTrackingWithOffset), '4,6,8,10');
    assert.sameValue(toLocaleStringHelper(lengthTrackingWithOffset), '4,6,8,10');
  }
}

JoinToLocaleString(TypedArrayJoinHelper, TypedArrayToLocaleStringHelper, true);
JoinToLocaleString(ArrayJoinHelper, ArrayToLocaleStringHelper, false);
