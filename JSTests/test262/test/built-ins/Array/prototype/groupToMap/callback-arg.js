// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.groupToMap
description: Array.prototype.groupToMap calls function with correct arguments
info: |
  22.1.3.15 Array.prototype.groupToMap ( callbackfn [ , thisArg ] )

  ...

  6. Repeat, while k < len
    a. Let Pk be ! ToString(𝔽(k)).
    b. Let kValue be ? Get(O, Pk).
    c. Let key be ? Call(callbackfn, thisArg, « kValue, 𝔽(k), O »).
    e. Perform ! AddValueToKeyedGroup(groups, key, kValue).
  ...
features: [array-grouping, Map]
---*/


const arr = [-0, 0, 1, 2, 3];

let calls = 0;

arr.groupToMap(function (n, i, testArr) {
  calls++;
  assert.sameValue(n, arr[i], "selected element aligns with index");
  assert.sameValue(testArr, arr, "original array is passed as final argument");
  return null;
});

assert.sameValue(calls, 5, 'called for all 5 elements');
