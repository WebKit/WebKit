// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from EntriesKeysValues test
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

function TypedArrayEntriesHelper(ta) {
  return ta.entries();
}

function ArrayEntriesHelper(ta) {
  return Array.prototype.entries.call(ta);
}

function ValuesFromTypedArrayEntries(ta) {
  let result = [];
  let expectedKey = 0;
  for (let [key, value] of ta.entries()) {
    assert.sameValue(key, expectedKey);
    ++expectedKey;
    result.push(Number(value));
  }
  return result;
}

function ValuesFromArrayEntries(ta) {
  let result = [];
  let expectedKey = 0;
  for (let [key, value] of Array.prototype.entries.call(ta)) {
    assert.sameValue(key, expectedKey);
    ++expectedKey;
    result.push(Number(value));
  }
  return result;
}

function TypedArrayKeysHelper(ta) {
  return ta.keys();
}

function ArrayKeysHelper(ta) {
  return Array.prototype.keys.call(ta);
}

function TypedArrayValuesHelper(ta) {
  return ta.values();
}

function ArrayValuesHelper(ta) {
  return Array.prototype.values.call(ta);
}

function ValuesFromTypedArrayValues(ta) {
  let result = [];
  for (let value of ta.values()) {
    result.push(Number(value));
  }
  return result;
}

function ValuesFromArrayValues(ta) {
  const result = [];
  for (let value of Array.prototype.values.call(ta)) {
    result.push(Number(value));
  }
  return result;
}

function EntriesKeysValues(entriesHelper, keysHelper, valuesHelper, valuesFromEntries, valuesFromValues, oobThrows) {
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

    assert.compareArray(valuesFromEntries(fixedLength), [
      0,
      2,
      4,
      6
    ]);
    assert.compareArray(valuesFromValues(fixedLength), [
      0,
      2,
      4,
      6
    ]);
    assert.compareArray(Array.from(keysHelper(fixedLength)), [
      0,
      1,
      2,
      3
    ]);
    assert.compareArray(valuesFromEntries(fixedLengthWithOffset), [
      4,
      6
    ]);
    assert.compareArray(valuesFromValues(fixedLengthWithOffset), [
      4,
      6
    ]);
    assert.compareArray(Array.from(keysHelper(fixedLengthWithOffset)), [
      0,
      1
    ]);
    assert.compareArray(valuesFromEntries(lengthTracking), [
      0,
      2,
      4,
      6
    ]);
    assert.compareArray(valuesFromValues(lengthTracking), [
      0,
      2,
      4,
      6
    ]);
    assert.compareArray(Array.from(keysHelper(lengthTracking)), [
      0,
      1,
      2,
      3
    ]);
    assert.compareArray(valuesFromEntries(lengthTrackingWithOffset), [
      4,
      6
    ]);
    assert.compareArray(valuesFromValues(lengthTrackingWithOffset), [
      4,
      6
    ]);
    assert.compareArray(Array.from(keysHelper(lengthTrackingWithOffset)), [
      0,
      1
    ]);

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    // TypedArray.prototype.{entries, keys, values} throw right away when
    // called. Array.prototype.{entries, keys, values} don't throw, but when
    // we try to iterate the returned ArrayIterator, that throws.
    if (oobThrows) {
      assert.throws(TypeError, () => {
        entriesHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        valuesHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        keysHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        entriesHelper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        valuesHelper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        keysHelper(fixedLengthWithOffset);
      });
    } else {
      entriesHelper(fixedLength);
      valuesHelper(fixedLength);
      keysHelper(fixedLength);
      entriesHelper(fixedLengthWithOffset);
      valuesHelper(fixedLengthWithOffset);
      keysHelper(fixedLengthWithOffset);
    }
    assert.throws(TypeError, () => {
      Array.from(entriesHelper(fixedLength));
    });
    assert.throws(TypeError, () => {
      Array.from(valuesHelper(fixedLength));
    });
    assert.throws(TypeError, () => {
      Array.from(keysHelper(fixedLength));
    });
    assert.throws(TypeError, () => {
      Array.from(entriesHelper(fixedLengthWithOffset));
    });
    assert.throws(TypeError, () => {
      Array.from(valuesHelper(fixedLengthWithOffset));
    });
    assert.throws(TypeError, () => {
      Array.from(keysHelper(fixedLengthWithOffset));
    });
    assert.compareArray(valuesFromEntries(lengthTracking), [
      0,
      2,
      4
    ]);
    assert.compareArray(valuesFromValues(lengthTracking), [
      0,
      2,
      4
    ]);
    assert.compareArray(Array.from(keysHelper(lengthTracking)), [
      0,
      1,
      2
    ]);
    assert.compareArray(valuesFromEntries(lengthTrackingWithOffset), [4]);
    assert.compareArray(valuesFromValues(lengthTrackingWithOffset), [4]);
    assert.compareArray(Array.from(keysHelper(lengthTrackingWithOffset)), [0]);

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        entriesHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        valuesHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        keysHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        entriesHelper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        valuesHelper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        keysHelper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        entriesHelper(lengthTrackingWithOffset);
      });
      assert.throws(TypeError, () => {
        valuesHelper(lengthTrackingWithOffset);
      });
      assert.throws(TypeError, () => {
        keysHelper(lengthTrackingWithOffset);
      });
    } else {
      entriesHelper(fixedLength);
      valuesHelper(fixedLength);
      keysHelper(fixedLength);
      entriesHelper(fixedLengthWithOffset);
      valuesHelper(fixedLengthWithOffset);
      keysHelper(fixedLengthWithOffset);
      entriesHelper(lengthTrackingWithOffset);
      valuesHelper(lengthTrackingWithOffset);
      keysHelper(lengthTrackingWithOffset);
    }
    assert.throws(TypeError, () => {
      Array.from(entriesHelper(fixedLength));
    });
    assert.throws(TypeError, () => {
      Array.from(valuesHelper(fixedLength));
    });
    assert.throws(TypeError, () => {
      Array.from(keysHelper(fixedLength));
    });
    assert.throws(TypeError, () => {
      Array.from(entriesHelper(fixedLengthWithOffset));
    });
    assert.throws(TypeError, () => {
      Array.from(valuesHelper(fixedLengthWithOffset));
    });
    assert.throws(TypeError, () => {
      Array.from(keysHelper(fixedLengthWithOffset));
    });
    assert.throws(TypeError, () => {
      Array.from(entriesHelper(lengthTrackingWithOffset));
    });
    assert.throws(TypeError, () => {
      Array.from(valuesHelper(lengthTrackingWithOffset));
    });
    assert.throws(TypeError, () => {
      Array.from(keysHelper(lengthTrackingWithOffset));
    });
    assert.compareArray(valuesFromEntries(lengthTracking), [0]);
    assert.compareArray(valuesFromValues(lengthTracking), [0]);
    assert.compareArray(Array.from(keysHelper(lengthTracking)), [0]);

    // Shrink to zero.
    rab.resize(0);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        entriesHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        valuesHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        keysHelper(fixedLength);
      });
      assert.throws(TypeError, () => {
        entriesHelper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        valuesHelper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        keysHelper(fixedLengthWithOffset);
      });
      assert.throws(TypeError, () => {
        entriesHelper(lengthTrackingWithOffset);
      });
      assert.throws(TypeError, () => {
        valuesHelper(lengthTrackingWithOffset);
      });
      assert.throws(TypeError, () => {
        keysHelper(lengthTrackingWithOffset);
      });
    } else {
      entriesHelper(fixedLength);
      valuesHelper(fixedLength);
      keysHelper(fixedLength);
      entriesHelper(fixedLengthWithOffset);
      valuesHelper(fixedLengthWithOffset);
      keysHelper(fixedLengthWithOffset);
      entriesHelper(lengthTrackingWithOffset);
      valuesHelper(lengthTrackingWithOffset);
      keysHelper(lengthTrackingWithOffset);
    }
    assert.throws(TypeError, () => {
      Array.from(entriesHelper(fixedLength));
    });
    assert.throws(TypeError, () => {
      Array.from(valuesHelper(fixedLength));
    });
    assert.throws(TypeError, () => {
      Array.from(keysHelper(fixedLength));
    });
    assert.throws(TypeError, () => {
      Array.from(entriesHelper(fixedLengthWithOffset));
    });
    assert.throws(TypeError, () => {
      Array.from(valuesHelper(fixedLengthWithOffset));
    });
    assert.throws(TypeError, () => {
      Array.from(keysHelper(fixedLengthWithOffset));
    });
    assert.throws(TypeError, () => {
      Array.from(entriesHelper(lengthTrackingWithOffset));
    });
    assert.throws(TypeError, () => {
      Array.from(valuesHelper(lengthTrackingWithOffset));
    });
    assert.throws(TypeError, () => {
      Array.from(keysHelper(lengthTrackingWithOffset));
    });
    assert.compareArray(valuesFromEntries(lengthTracking), []);
    assert.compareArray(valuesFromValues(lengthTracking), []);
    assert.compareArray(Array.from(keysHelper(lengthTracking)), []);

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

    assert.compareArray(valuesFromEntries(fixedLength), [
      0,
      2,
      4,
      6
    ]);
    assert.compareArray(valuesFromValues(fixedLength), [
      0,
      2,
      4,
      6
    ]);
    assert.compareArray(Array.from(keysHelper(fixedLength)), [
      0,
      1,
      2,
      3
    ]);
    assert.compareArray(valuesFromEntries(fixedLengthWithOffset), [
      4,
      6
    ]);
    assert.compareArray(valuesFromValues(fixedLengthWithOffset), [
      4,
      6
    ]);
    assert.compareArray(Array.from(keysHelper(fixedLengthWithOffset)), [
      0,
      1
    ]);
    assert.compareArray(valuesFromEntries(lengthTracking), [
      0,
      2,
      4,
      6,
      8,
      10
    ]);
    assert.compareArray(valuesFromValues(lengthTracking), [
      0,
      2,
      4,
      6,
      8,
      10
    ]);
    assert.compareArray(Array.from(keysHelper(lengthTracking)), [
      0,
      1,
      2,
      3,
      4,
      5
    ]);
    assert.compareArray(valuesFromEntries(lengthTrackingWithOffset), [
      4,
      6,
      8,
      10
    ]);
    assert.compareArray(valuesFromValues(lengthTrackingWithOffset), [
      4,
      6,
      8,
      10
    ]);
    assert.compareArray(Array.from(keysHelper(lengthTrackingWithOffset)), [
      0,
      1,
      2,
      3
    ]);
  }
}

EntriesKeysValues(TypedArrayEntriesHelper, TypedArrayKeysHelper, TypedArrayValuesHelper, ValuesFromTypedArrayEntries, ValuesFromTypedArrayValues, true);
EntriesKeysValues(ArrayEntriesHelper, ArrayKeysHelper, ArrayValuesHelper, ValuesFromArrayEntries, ValuesFromArrayValues, false);
