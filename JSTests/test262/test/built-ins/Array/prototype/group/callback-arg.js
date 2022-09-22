// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.group
description: Array.prototype.group calls function with correct arguments
info: |
  22.1.3.14 Array.prototype.group ( callbackfn [ , thisArg ] )

  ...

  6. Repeat, while k < len

      a. Let Pk be ! ToString(𝔽(k)).
      b. Let kValue be ? Get(O, Pk).
      c. Let propertyKey be ? ToPropertyKey(? Call(callbackfn, thisArg, « kValue, 𝔽(k), O »)).
      d. Perform ! AddValueToKeyedGroup(groups, propertyKey, kValue).
      e. Set k to k + 1.
  ...
features: [array-grouping]
---*/


const arr = [-0, 0, 1, 2, 3];

let calls = 0;

const map = arr.group(function (n, i, testArr) {
  calls++;
  assert.sameValue(n, arr[i], "selected element aligns with index");
  assert.sameValue(testArr, arr, "original array is passed as final argument");
  return null;
});

assert.sameValue(calls, 5, 'called for all 5 elements');
