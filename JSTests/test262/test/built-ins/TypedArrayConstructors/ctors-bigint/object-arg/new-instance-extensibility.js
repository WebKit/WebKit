// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-typedarray-object
description: >
  The new typedArray instance from an object argument is extensible
info: |
  22.2.4.4 TypedArray ( object )

  ...
  4. Let O be ? AllocateTypedArray(constructorName, NewTarget,
  "%TypedArrayPrototype%").
  ...

  22.2.4.2.1 Runtime Semantics: AllocateTypedArray (constructorName, newTarget,
  defaultProto [ , length ])

  ...
  2. Let obj be IntegerIndexedObjectCreate(proto, « [[ViewedArrayBuffer]],
  [[TypedArrayName]], [[ByteLength]], [[ByteOffset]], [[ArrayLength]] »).
  ...

  9.4.5.7 IntegerIndexedObjectCreate (prototype, internalSlotsList)

  ...
  11. Set the [[Extensible]] internal slot of A to true.
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var obj = {
    "0": 0n,
    "1": 1n,
    "2": 2n,
    length: 3
  };

  var sample = new TA(obj);

  assert(Object.isExtensible(sample));
});
