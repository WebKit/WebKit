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

const arr = ['hello', 'test', 'world'];

const map = arr.groupToMap(function (i) { return i.length; });

assert.compareArray(Array.from(map.keys()), [5, 4]);
assert.compareArray(map.get(5), ['hello', 'world']);
assert.compareArray(map.get(4), ['test']);
