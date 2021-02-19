// Copyright 2019 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: The "indices" property is created with DefinePropertyOrThrow
includes: [propertyHelper.js]
esid: sec-regexpbuiltinexec
features: [regexp-match-indices]
info: |
  Runtime Semantics: RegExpBuiltinExec ( R, S )
    8. If _flags_ contains `"d"`, let _hasIndices_ be *true*, else let _hasIndices_ be *false*.
    ...
    36. If _hasIndices_ is *true*, then
      a. Let _indicesArray_ be MakeIndicesArray(_S_, _indices_, _groupNames_, _hasGroups_).
      b. Perform ! CreateDataProperty(_A_, `"indices"`, _indicesArray_).
---*/

function assertSameValue(a, b)
{
    if (a !== b)
        throw "Values not same";
}

function verifyProperty(obj, prop, expect)
{
    let desc = Object.getOwnPropertyDescriptor(obj, prop);

    for (const [key, value] of Object.entries(expect))
        if (desc[key] != value)
            throw "obj." + prop + " " + key + ": is not " + value;
}

// `indices` is created with Define, not Set.
let counter = 0;
Object.defineProperty(Array.prototype, "indices", {
  set() { counter++; }
});

let match = /a/d.exec("a");
assertSameValue(counter, 0);

// `indices` is a non-writable, non-enumerable, and configurable data-property.
verifyProperty(match, 'indices', {
  writable: true,
  enumerable: true,
  configurable: true
});
