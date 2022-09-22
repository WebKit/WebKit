// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from SortCallbackGrows test
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

const TypedArraySortHelper = (ta, ...rest) => {
  ta.sort(...rest);
};

const ArraySortHelper = (ta, ...rest) => {
  Array.prototype.sort.call(ta, ...rest);
};

function SortCallbackGrows(sortHelper) {
  function WriteUnsortedData(taFull) {
    for (let i = 0; i < taFull.length; ++i) {
      WriteToTypedArray(taFull, i, 10 - i);
    }
  }
  let rab;
  let resizeTo;
  function CustomComparison(a, b) {
    rab.resize(resizeTo);
    if (a < b) {
      return -1;
    }
    if (a > b) {
      return 1;
    }
    return 0;
  }

  // Fixed length TA.
  for (let ctor of ctors) {
    rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    resizeTo = 6 * ctor.BYTES_PER_ELEMENT;
    const fixedLength = new ctor(rab, 0, 4);
    const taFull = new ctor(rab, 0);
    WriteUnsortedData(taFull);
    sortHelper(fixedLength, CustomComparison);
    // Growing doesn't affect the sorting.
    assert.compareArray(ToNumbers(taFull), [
      7,
      8,
      9,
      10,
      0,
      0
    ]);
  }

  // Length-tracking TA.
  for (let ctor of ctors) {
    rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    resizeTo = 6 * ctor.BYTES_PER_ELEMENT;
    const lengthTracking = new ctor(rab, 0);
    const taFull = new ctor(rab, 0);
    WriteUnsortedData(taFull);
    sortHelper(lengthTracking, CustomComparison);
    // Growing doesn't affect the sorting. Only the elements that were part of
    // the original TA are sorted.
    assert.compareArray(ToNumbers(taFull), [
      7,
      8,
      9,
      10,
      0,
      0
    ]);
  }
}

SortCallbackGrows(TypedArraySortHelper);
SortCallbackGrows(ArraySortHelper);
