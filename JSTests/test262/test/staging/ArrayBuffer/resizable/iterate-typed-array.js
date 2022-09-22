// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from IterateTypedArray test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
features: [resizable-arraybuffer]
includes: [compareArray.js]
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

const no_elements = 10;
const offset = 2;
function TestIteration(ta, expected) {
  let values = [];
  for (const value of ta) {
    values.push(Number(value));
  }
  assert.compareArray(values, expected);
}
for (let ctor of ctors) {
  const buffer_byte_length = no_elements * ctor.BYTES_PER_ELEMENT;
  // We can use the same RAB for all the TAs below, since we won't modify it
  // after writing the initial values.
  const rab = CreateResizableArrayBuffer(buffer_byte_length, 2 * buffer_byte_length);
  const byte_offset = offset * ctor.BYTES_PER_ELEMENT;

  // Write some data into the array.
  let ta_write = new ctor(rab);
  for (let i = 0; i < no_elements; ++i) {
    WriteToTypedArray(ta_write, i, i % 128);
  }

  // Create various different styles of TypedArrays with the RAB as the
  // backing store and iterate them.
  const ta = new ctor(rab, 0, 3);
  TestIteration(ta, [
    0,
    1,
    2
  ]);
  const empty_ta = new ctor(rab, 0, 0);
  TestIteration(empty_ta, []);
  const ta_with_offset = new ctor(rab, byte_offset, 3);
  TestIteration(ta_with_offset, [
    2,
    3,
    4
  ]);
  const empty_ta_with_offset = new ctor(rab, byte_offset, 0);
  TestIteration(empty_ta_with_offset, []);
  const length_tracking_ta = new ctor(rab);
  {
    let expected = [];
    for (let i = 0; i < no_elements; ++i) {
      expected.push(i % 128);
    }
    TestIteration(length_tracking_ta, expected);
  }
  const length_tracking_ta_with_offset = new ctor(rab, byte_offset);
  {
    let expected = [];
    for (let i = offset; i < no_elements; ++i) {
      expected.push(i % 128);
    }
    TestIteration(length_tracking_ta_with_offset, expected);
  }
  const empty_length_tracking_ta_with_offset = new ctor(rab, buffer_byte_length);
  TestIteration(empty_length_tracking_ta_with_offset, []);
}
