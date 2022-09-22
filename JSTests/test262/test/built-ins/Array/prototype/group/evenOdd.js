// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.group
description: Array.prototype.group populates object with correct keys and values
info: |
  22.1.3.14 Array.prototype.group ( callbackfn [ , thisArg ] )

  ...

  8. For each Record { [[Key]], [[Elements]] } g of groups, do

    a. Let elements be ! CreateArrayFromList(g.[[Elements]]).
    b. Perform ! CreateDataPropertyOrThrow(obj, g.[[Key]], elements).

  ...
includes: [compareArray.js]
features: [array-grouping]
---*/

const array = [1, 2, 3];

const obj = array.group(function (i) {
  return i % 2 === 0 ? 'even' : 'odd';
});

assert.compareArray(Object.keys(obj), ['odd', 'even']);
assert.compareArray(obj['even'], [2]);
assert.compareArray(obj['odd'], [1, 3]);
