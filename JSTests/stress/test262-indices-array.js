// Copyright 2019 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: The "indices" property is an Array.
esid: sec-makeindicesarray
features: [regexp-match-indices]
info: |
  MakeIndicesArray ( S, indices, groupNames, hasGroups )
    6. Set _A_ to ! ArrayCreate(_n_).
---*/

function assert(a)
{
    if (!a)
        throw "Values not true";
}

function assertSameValue(a, b)
{
    if (a !== b)
        throw "Values not same";
}

let match = /a/d.exec("a");
let indices = match.indices;

// `indices` is an array
assertSameValue(Object.getPrototypeOf(indices), Array.prototype);
assert(Array.isArray(indices));
