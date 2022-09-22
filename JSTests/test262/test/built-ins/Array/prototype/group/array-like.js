// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.group
description: Array.prototype.group iterates array-like up to length
info: |
  22.1.3.14 Array.prototype.group ( callbackfn [ , thisArg ] )

  ...

  2. Let len be ? LengthOfArrayLike(O).
  ...
  4. Let k be 0.
  ...
  6. Repeat, while k < len

  ...
includes: [compareArray.js]
features: [array-grouping]
---*/

const arrayLike = {0: 1, 1: 2, 2: 3, 3: 4, length: 3 };

let calls = 0;

const obj = Array.prototype.group.call(arrayLike, function (i) { calls++; return i % 2 === 0 ? 'even' : 'odd'; });

assert.sameValue(calls, 3, 'only calls length times');
assert.compareArray(Object.keys(obj), ['odd', 'even']);
assert.compareArray(obj['even'], [2]);
assert.compareArray(obj['odd'], [1, 3]);
