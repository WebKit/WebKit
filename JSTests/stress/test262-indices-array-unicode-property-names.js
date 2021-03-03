// Copyright 2019 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Basic matching cases with non-unicode matches.
includes: [compareArray.js]
esid: sec-makeindicesarray
features: [regexp-match-indices]
---*/

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

assertCompareArray([1, 2], /(?<π>a)/du.exec("bab").indices.groups.π);
assertCompareArray([1, 2], /(?<\u{03C0}>a)/du.exec("bab").indices.groups.π);
assertCompareArray([1, 2], /(?<π>a)/du.exec("bab").indices.groups.\u03C0);
assertCompareArray([1, 2], /(?<\u{03C0}>a)/du.exec("bab").indices.groups.\u03C0);
assertCompareArray([1, 2], /(?<$>a)/du.exec("bab").indices.groups.$);
assertCompareArray([1, 2], /(?<_>a)/du.exec("bab").indices.groups._);
assertCompareArray([1, 2], /(?<$𐒤>a)/du.exec("bab").indices.groups.$𐒤);
assertCompareArray([1, 2], /(?<_\u200C>a)/du.exec("bab").indices.groups._\u200C);
assertCompareArray([1, 2], /(?<_\u200D>a)/du.exec("bab").indices.groups._\u200D);
assertCompareArray([1, 2], /(?<ಠ_ಠ>a)/du.exec("bab").indices.groups.ಠ_ಠ);
