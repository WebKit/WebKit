// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.groupToMap
description: Array.prototype.groupToMap populates object with correct keys and values
info: |
  22.1.3.15 Array.prototype.groupToMap ( callbackfn [ , thisArg ] )

  ...

  8. For each Record { [[Key]], [[Elements]] } g of groups, do

    a. Let elements be ! CreateArrayFromList(g.[[Elements]]).
    b. Perform ! CreateDataPropertyOrThrow(map, g.[[Key]], elements).

  ...
includes: [compareArray.js]
features: [array-grouping, Map, Symbol.iterator]
---*/

const array = [1, 2, 3];

const map = array.groupToMap(function (i) {
  return i % 2 === 0 ? 'even' : 'odd';
});

assert.compareArray(Array.from(map.keys()), ['odd', 'even']);
assert.compareArray(map.get('even'), [2]);
assert.compareArray(map.get('odd'), [1, 3]);
