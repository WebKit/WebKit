// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from IterateTypedArrayAndShrinkMidIteration test
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

function CreateRab(buffer_byte_length, ctor) {
  const rab = CreateResizableArrayBuffer(buffer_byte_length, 2 * buffer_byte_length);
  let ta_write = new ctor(rab);
  for (let i = 0; i < buffer_byte_length / ctor.BYTES_PER_ELEMENT; ++i) {
    WriteToTypedArray(ta_write, i, i % 128);
  }
  return rab;
}

function TestIterationAndResize(ta, expected, rab, resize_after, new_byte_length) {
  let values = [];
  let resized = false;
  for (const value of ta) {
    if (value instanceof Array) {
      values.push([
        value[0],
        Number(value[1])
      ]);
    } else {
      values.push(Number(value));
    }
    if (!resized && values.length == resize_after) {
      rab.resize(new_byte_length);
      resized = true;
    }
  }
  assert.compareArray(values, expected);
  assert(resized);
}

const no_elements = 10;
const offset = 2;
for (let ctor of ctors) {
  const buffer_byte_length = no_elements * ctor.BYTES_PER_ELEMENT;
  const byte_offset = offset * ctor.BYTES_PER_ELEMENT;

  // Create various different styles of TypedArrays with the RAB as the
  // backing store and iterate them.

  // Fixed-length TAs aren't affected by shrinking if they stay in-bounds.
  // They appear detached after shrinking out of bounds.
  let rab = CreateRab(buffer_byte_length, ctor);
  const ta1 = new ctor(rab, 0, 3);
  TestIterationAndResize(ta1, [
    0,
    1,
    2
  ], rab, 2, buffer_byte_length / 2);
  rab = CreateRab(buffer_byte_length, ctor);
  const ta2 = new ctor(rab, 0, 3);
  assert.throws(TypeError, () => {
    TestIterationAndResize(ta2, null, rab, 2, 1);
  });
  rab = CreateRab(buffer_byte_length, ctor);
  const ta_with_offset1 = new ctor(rab, byte_offset, 3);
  TestIterationAndResize(ta_with_offset1, [
    2,
    3,
    4
  ], rab, 2, buffer_byte_length / 2);
  rab = CreateRab(buffer_byte_length, ctor);
  const ta_with_offset2 = new ctor(rab, byte_offset, 3);
  assert.throws(TypeError, () => {
    TestIterationAndResize(ta_with_offset2, null, rab, 2, 0);
  });

  // Length-tracking TA with offset 0 doesn't throw, but its length gracefully
  // reduces too.
  rab = CreateRab(buffer_byte_length, ctor);
  const length_tracking_ta = new ctor(rab);
  TestIterationAndResize(length_tracking_ta, [
    0,
    1,
    2,
    3,
    4
  ], rab, 2, buffer_byte_length / 2);

  // Length-tracking TA appears detached when the buffer is resized beyond the
  // offset.
  rab = CreateRab(buffer_byte_length, ctor);
  const length_tracking_ta_with_offset = new ctor(rab, byte_offset);
  assert.throws(TypeError, () => {
    TestIterationAndResize(length_tracking_ta_with_offset, null, rab, 2, byte_offset / 2);
  });

  // Length-tracking TA reduces its length gracefully when the buffer is
  // resized to barely cover the offset.
  rab = CreateRab(buffer_byte_length, ctor);
  const length_tracking_ta_with_offset2 = new ctor(rab, byte_offset);
  TestIterationAndResize(length_tracking_ta_with_offset2, [
    2,
    3
  ], rab, 2, byte_offset);
}
