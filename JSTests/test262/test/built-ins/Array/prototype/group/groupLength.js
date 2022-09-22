// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.group
description: Callback can return numbers that are converted to property keys
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

const arr = ['hello', 'test', 'world'];

const obj = arr.group(function (i) { return i.length; });

assert.compareArray(Object.keys(obj), ['4', '5']);
assert.compareArray(obj['5'], ['hello', 'world']);
assert.compareArray(obj['4'], ['test']);
