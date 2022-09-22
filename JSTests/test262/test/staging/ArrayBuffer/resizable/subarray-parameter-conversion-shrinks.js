// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from SubarrayParameterConversionShrinks test
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

// Orig. array: [0, 2, 4, 6]
//              [0, 2, 4, 6] << fixedLength
//              [0, 2, 4, 6, ...] << lengthTracking
function CreateRabForTest(ctor) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  // Write some data into the array.
  const taWrite = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(taWrite, i, 2 * i);
  }
  return rab;
}

// Fixed-length TA + first parameter conversion shrinks. The old length is
// used in the length computation, and the subarray construction fails.
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  let evil = {
    valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }
  };
  assert.throws(RangeError, () => {
    fixedLength.subarray(evil);
  });
}

// Like the previous test, but now we construct a smaller subarray and it
// succeeds.
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  let evil = {
    valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }
  };
  assert.compareArray(ToNumbers(fixedLength.subarray(evil, 1)), [0]);
}

// Fixed-length TA + second parameter conversion shrinks. The old length is
// used in the length computation, and the subarray construction fails.
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  let evil = {
    valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 3;
    }
  };
  assert.throws(RangeError, () => {
    fixedLength.subarray(0, evil);
  });
}

// Like the previous test, but now we construct a smaller subarray and it
// succeeds.
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  let evil = {
    valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 1;
    }
  };
  assert.compareArray(ToNumbers(fixedLength.subarray(0, evil)), [0]);
}

// Shrinking + fixed-length TA, subarray construction succeeds even though the
// TA goes OOB.
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  const evil = {
    valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }
  };
  assert.compareArray(ToNumbers(fixedLength.subarray(evil, 1)), [0]);
}

// Length-tracking TA + first parameter conversion shrinks. The old length is
// used in the length computation, and the subarray construction fails.
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab);
  let evil = {
    valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }
  };
  assert.throws(RangeError, () => {
    lengthTracking.subarray(evil, lengthTracking.length);
  });
}

// Like the previous test, but now we construct a smaller subarray and it
// succeeds.
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab);
  let evil = {
    valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }
  };
  assert.compareArray(ToNumbers(lengthTracking.subarray(evil, 1)), [0]);
}

// Length-tracking TA + first parameter conversion shrinks. The second
// parameter is negative -> the relative index is not recomputed, and the
// subarray construction fails.
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab);
  let evil = {
    valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }
  };
  assert.throws(RangeError, () => {
    lengthTracking.subarray(evil, -1);
  });
}

// Length-tracking TA + second parameter conversion shrinks. The second
// parameter is too large -> the subarray construction fails.
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab);
  let evil = {
    valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 3;
    }
  };
  assert.throws(RangeError, () => {
    lengthTracking.subarray(0, evil);
  });
}
