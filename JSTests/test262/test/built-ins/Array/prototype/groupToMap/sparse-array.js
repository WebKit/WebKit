// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.groupToMap
description: Array.prototype.groupToMap adds undefined to the group for sparse arrays
info: |
  22.1.3.15 Array.prototype.groupToMap ( callbackfn [ , thisArg ] )

  ...

  6. Repeat, while k < len
    a. Let Pk be ! ToString(𝔽(k)).
    b. Let kValue be ? Get(O, Pk).
    c. Let key be ? Call(callbackfn, thisArg, « kValue, 𝔽(k), O »).
    e. Perform AddValueToKeyedGroup(groups, key, kValue).

  ...
includes: [compareArray.js]
features: [array-grouping, Map]
---*/

let calls = 0;
const array = [, , ,];

const map = array.groupToMap(function () {
  calls++;
  return 'key';
});

assert.sameValue(calls, 3);

const key = map.get('key');
assert(0 in key, 'group has a first element');
assert(1 in key, 'group has a second element');
assert(2 in key, 'group has a third element');
assert.compareArray(key, [void 0, void 0, void 0]);
