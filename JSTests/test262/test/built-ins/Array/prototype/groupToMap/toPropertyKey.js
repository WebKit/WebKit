// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.groupToMap
description: Array.prototype.groupToMap does not coerce return value with ToPropertyKey
info: |
  22.1.3.15 Array.prototype.groupToMap ( callbackfn [ , thisArg ] )

  ...

  6. Repeat, while k < len
    c. Let key be ? Call(callbackfn, thisArg, « kValue, 𝔽(k), O »).
    e. Perform AddValueToKeyedGroup(groups, key, kValue).
  ...
  8. For each Record { [[Key]], [[Elements]] } g of groups, do
    a. Let elements be ! CreateArrayFromList(g.[[Elements]]).
    b. Perform ! CreateDataPropertyOrThrow(map, g.[[Key]], elements).

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

const map = array.groupToMap(v => v);

assert.compareArray(Array.from(map.keys()), [1, '1', stringable]);
assert.compareArray(map.get('1'), ['1']);
assert.compareArray(map.get(1), [1]);
assert.compareArray(map.get(stringable), [stringable]);
