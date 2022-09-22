// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from ToLocaleStringNumberPrototypeToLocaleStringShrinks test
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

const TypedArrayToLocaleStringHelper = (ta, ...rest) => {
  return ta.toLocaleString(...rest);
};

const ArrayToLocaleStringHelper = (ta, ...rest) => {
  return Array.prototype.toLocaleString.call(ta, ...rest);
};

function ToLocaleStringNumberPrototypeToLocaleStringShrinks(toLocaleStringHelper) {
  const oldNumberPrototypeToLocaleString = Number.prototype.toLocaleString;
  const oldBigIntPrototypeToLocaleString = BigInt.prototype.toLocaleString;

  // Shrinking + fixed-length TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    let resizeAfter = 2;
    Number.prototype.toLocaleString = function () {
      --resizeAfter;
      if (resizeAfter == 0) {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      }
      return oldNumberPrototypeToLocaleString.call(this);
    };
    BigInt.prototype.toLocaleString = function () {
      --resizeAfter;
      if (resizeAfter == 0) {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      }
      return oldBigIntPrototypeToLocaleString.call(this);
    };

    // We iterate 4 elements, since it was the starting length. The TA goes
    // OOB after 2 elements.
    assert.sameValue(toLocaleStringHelper(fixedLength), '0,0,,');
  }

  // Shrinking + length-tracking TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    let resizeAfter = 2;
    Number.prototype.toLocaleString = function () {
      --resizeAfter;
      if (resizeAfter == 0) {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      }
      return oldNumberPrototypeToLocaleString.call(this);
    };
    BigInt.prototype.toLocaleString = function () {
      --resizeAfter;
      if (resizeAfter == 0) {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      }
      return oldBigIntPrototypeToLocaleString.call(this);
    };

    // We iterate 4 elements, since it was the starting length. Elements beyond
    // the new length are converted to the empty string.
    assert.sameValue(toLocaleStringHelper(lengthTracking), '0,0,,');
  }
  Number.prototype.toLocaleString = oldNumberPrototypeToLocaleString;
  BigInt.prototype.toLocaleString = oldBigIntPrototypeToLocaleString;
}

ToLocaleStringNumberPrototypeToLocaleStringShrinks(TypedArrayToLocaleStringHelper);
ToLocaleStringNumberPrototypeToLocaleStringShrinks(ArrayToLocaleStringHelper);
