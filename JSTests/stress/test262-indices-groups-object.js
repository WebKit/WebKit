// Copyright 2019 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: The groups object of indices is created with CreateDataProperty
includes: [propertyHelper.js, compareArray.js]
esid: sec-makeindicesarray
features: [regexp-named-groups, regexp-match-indices]
info: |
  MakeIndicesArray ( S, indices, groupNames, hasIndices )
    10. If _hasIndices_ is *true*, then
      a. Let _groups_ be ! ObjectCreate(*null*).
    11. Else,
      a. Let _groups_ be *undefined*.
    12. Perform ! CreateDataProperty(_A_, `"groups"`, _groups_).
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

function verifyProperty(obj, prop, expect)
{
    let desc = Object.getOwnPropertyDescriptor(obj, prop);

    for (const [key, value] of Object.entries(expect))
        if (desc[key] != value)
            throw "obj." + prop + " " + key + ": is not " + value;
}

// `groups` is created with Define, not Set.
let counter = 0;
Object.defineProperty(Array.prototype, "groups", {
  set() { counter++; }
});

let indices = /(?<x>.)/d.exec("a").indices;
assertSameValue(counter, 0);

// `groups` is writable, enumerable and configurable
// (from CreateDataProperty).
verifyProperty(indices, 'groups', {
    writable: true,
    enumerable: true,
    configurable: true
});

// The `__proto__` property on the groups object is not special,
// and does not affect the [[Prototype]] of the resulting groups object.
let {groups} = /(?<__proto__>.)/d.exec("a").indices;
assertCompareArray([0, 1], groups.__proto__);
assertSameValue(null, Object.getPrototypeOf(groups));
