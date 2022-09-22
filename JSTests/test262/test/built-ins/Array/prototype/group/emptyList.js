// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.group
description: Callback is not called and object is not populated if the array is empty
info: |
  22.1.3.14 Array.prototype.group ( callbackfn [ , thisArg ] )

  ...

  8. For each Record { [[Key]], [[Elements]] } g of groups, do

    a. Let elements be ! CreateArrayFromList(g.[[Elements]]).
    b. Perform ! CreateDataPropertyOrThrow(obj, g.[[Key]], elements).

  ...
features: [array-grouping]
---*/

const original = [];

const obj = original.group(function () {
  throw new Test262Error('callback function should not be called')
});

assert.notSameValue(original, obj, 'group returns a object');
assert.sameValue(Object.keys(obj).length, 0);
