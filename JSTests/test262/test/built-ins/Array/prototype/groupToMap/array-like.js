// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.groupToMap
description: Array.prototype.groupToMap iterates array-like up to length
info: |
  22.1.3.15 Array.prototype.groupToMap ( callbackfn [ , thisArg ] )

  ...

  2. Let len be ? LengthOfArrayLike(O).
  ...
  4. Let k be 0.
  ...
  6. Repeat, while k < len

  ...
includes: [compareArray.js]
features: [array-grouping, Map, Symbol.iterator]
---*/

const arrayLike = {0: 1, 1: 2, 2: 3, 3: 4, length: 3 };

let calls = 0;

const map = Array.prototype.groupToMap.call(arrayLike, function (i) {
  calls++;
  return i % 2 === 0 ? 'even' : 'odd';
});

assert.sameValue(calls, 3, 'only calls length times');
assert.compareArray(Array.from(map.keys()), ['odd', 'even']);
assert.compareArray(map.get('even'), [2]);
assert.compareArray(map.get('odd'), [1, 3]);
