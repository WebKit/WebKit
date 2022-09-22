// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.groupToMap
description: Callback is not called and object is not populated if the array is empty
info: |
  22.1.3.15 Array.prototype.groupToMap ( callbackfn [ , thisArg ] )

  ...

  8. For each Record { [[Key]], [[Elements]] } g of groups, do

    a. Let elements be ! CreateArrayFromList(g.[[Elements]]).
    b. Perform ! CreateDataPropertyOrThrow(map, g.[[Key]], elements).

  ...
features: [array-grouping]
---*/

const original = [];

const map = original.groupToMap(function () {
  throw new Test262Error('callback function should not be called')
});

assert.notSameValue(original, map, 'groupToMap returns a map');
assert.sameValue(map.size, 0);
