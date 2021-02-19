// Copyright 2019 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Test the groups object of indices with matched and unmatched named captures.
includes: [compareArray.js]
esid: sec-makeindicesarray
features: [regexp-named-groups, regexp-match-indices]
info: |
  MakeIndicesArray ( S, indices, groupNames, hasGroups )
    11. For each integer _i_ such that _i_ >= 0 and _i_ < _n_, do
      e. If _i_ > 0 and _groupNames_[_i_ - 1] is not *undefined*, then
        i. Perform ! CreateDataProperty(_groups_, _groupNames_[_i_ - 1], _matchIndicesArray_).
---*/

function assertSameValue(a, b)
{
    if (a !== b)
        throw "Values not same";
}

function assertCompareArray(a, b)
{
    if (!a instanceof Array)
        throw "a not array";

    if (!b instanceof Array)
        throw "b not array";

    if (a.length !== b.length)
        throw "Arrays differ in length";

    for (let i = 0; i < a.length; ++i)
        if (a[i] !== b[i])
            throw "Array element " + i + " differ";
}

const re = /(?<a>a).|(?<x>x)/d;
const result = re.exec("ab").indices;
assertCompareArray([0, 1], result.groups.a);
assertSameValue(undefined, result.groups.x);
