// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.reverse
description: >
  Ensure correct MOP operations are called when length exceeds 2^53-1.
includes: [compareArray.js, proxyTrapsHelper.js]
---*/

function StopReverse() {}

var arrayLike = {
  0: "zero",
  /* 1: hole, */
  2: "two",
  /* 3: hole, */

  get 4() {
    throw new StopReverse();
  },

  9007199254740987: "2**53-5",
  /* 9007199254740988: hole, */
  /* 9007199254740989: hole, */
  9007199254740990: "2**53-2",

  length: 2 ** 53 + 2,
};

var traps = [];

var proxy = new Proxy(arrayLike, allowProxyTraps({
  getOwnPropertyDescriptor(t, pk) {
    traps.push(`GetOwnPropertyDescriptor:${String(pk)}`);
    return Reflect.getOwnPropertyDescriptor(t, pk);
  },
  defineProperty(t, pk, desc) {
    traps.push(`DefineProperty:${String(pk)}`);
    return Reflect.defineProperty(t, pk, desc);
  },
  has(t, pk) {
    traps.push(`Has:${String(pk)}`);
    return Reflect.has(t, pk);
  },
  get(t, pk, r) {
    traps.push(`Get:${String(pk)}`);
    return Reflect.get(t, pk, r);
  },
  set(t, pk, v, r) {
    traps.push(`Set:${String(pk)}`);
    return Reflect.set(t, pk, v, r);
  },
  deleteProperty(t, pk) {
    traps.push(`Delete:${String(pk)}`);
    return Reflect.deleteProperty(t, pk);
  },
}))

// Uses a separate exception than Test262Error, so that errors from allowProxyTraps
// are properly propagated.
assert.throws(StopReverse, function() {
  Array.prototype.reverse.call(proxy);
});

assert.compareArray(traps, [
  // Initial get length operation.
  "Get:length",

  // Lower and upper index are both present.
  "Has:0",
  "Get:0",
  "Has:9007199254740990",
  "Get:9007199254740990",
  "Set:0",
  "GetOwnPropertyDescriptor:0",
  "DefineProperty:0",
  "Set:9007199254740990",
  "GetOwnPropertyDescriptor:9007199254740990",
  "DefineProperty:9007199254740990",

  // Lower and upper index are both absent.
  "Has:1",
  "Has:9007199254740989",

  // Lower index is present, upper index is absent.
  "Has:2",
  "Get:2",
  "Has:9007199254740988",
  "Delete:2",
  "Set:9007199254740988",
  "GetOwnPropertyDescriptor:9007199254740988",
  "DefineProperty:9007199254740988",

  // Lower index is absent, upper index is present.
  "Has:3",
  "Has:9007199254740987",
  "Get:9007199254740987",
  "Set:3",
  "GetOwnPropertyDescriptor:3",
  "DefineProperty:3",
  "Delete:9007199254740987",

  // Stop exception.
  "Has:4",
  "Get:4",
]);

assert.sameValue(arrayLike.length, 2 ** 53 + 2, "Length property is not modified");

assert.sameValue(arrayLike[0], "2**53-2", "Property at index 0");
assert.sameValue(1 in arrayLike, false, "Property at index 1");
assert.sameValue(2 in arrayLike, false, "Property at index 2");
assert.sameValue(arrayLike[3], "2**53-5", "Property at index 3");

assert.sameValue(9007199254740987 in arrayLike, false, "Property at index 2**53-5");
assert.sameValue(arrayLike[9007199254740988], "two", "Property at index 2**53-4");
assert.sameValue(9007199254740989 in arrayLike, false, "Property at index 2**53-3");
assert.sameValue(arrayLike[9007199254740990], "zero", "Property at index 2**53-2");
