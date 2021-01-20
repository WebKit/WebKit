// Copyright 2019 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Test the groups object of indices with matched and unmatched named captures.
includes: [compareArray.js]
esid: sec-makeindicesarray
features: [regexp-named-groups, regexp-match-indices]
info: |
  MakeIndicesArray ( S, indices, groupNames )
    11. For each integer _i_ such that _i_ >= 0 and _i_ < _n_, do
      e. If _groupNames_ is not *undfined* and _groupNames_[_i_] is not *undefined*, then
        i. Perform ! CreateDataProperty(_groups_, _groupNames_[_i_], _matchIndicesArray_).
---*/


const re = /(?<a>a).|(?<x>x)/;
const result = re.exec("ab").indices;
assert.compareArray([0, 1], result.groups.a);
assert.sameValue(undefined, result.groups.x);
