// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.group
description: Array.prototype.group coerces return value with ToPropertyKey
info: |
  22.1.3.14 Array.prototype.group ( callbackfn [ , thisArg ] )

  ...

  6. Repeat, while k < len
    c. Let propertyKey be ? ToPropertyKey(? Call(callbackfn, thisArg, « kValue, 𝔽(k), O »)).
    d. Perform AddValueToKeyedGroup(groups, propertyKey, kValue).
  ...
  8. For each Record { [[Key]], [[Elements]] } g of groups, do
    a. Let elements be ! CreateArrayFromList(g.[[Elements]]).
    b. Perform ! CreateDataPropertyOrThrow(obj, g.[[Key]], elements).

  ...
includes: [compareArray.js]
features: [array-grouping]
---*/

let calls = 0;
const stringable = {
  toString() {
    return 1;
  }
}

const array = [1, '1', stringable];

const obj = array.group(function (v) { return v; });

assert.compareArray(Object.keys(obj), ['1']);
assert.compareArray(obj['1'], [1, '1', stringable]);
