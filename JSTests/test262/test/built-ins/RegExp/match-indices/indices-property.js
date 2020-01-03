// Copyright 2019 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: The "indices" property is created with DefinePropertyOrThrow
includes: [propertyHelper.js]
esid: sec-regexpbuiltinexec
features: [regexp-match-indices]
info: |
  Runtime Semantics: RegExpBuiltinExec ( R, S )
    34. Let _indicesArray_ be MakeIndicesArray(_S_, _indices_, _groupNames_).
    35. Perform ! DefinePropertyOrThrow(_A_, `"indices"`, PropertyDescriptor { [[Value]]: _indicesArray_, [[Writable]]: *false*, [[Enumerable]]: *false*, [[Configurable]]: *true* }).
---*/

// `indices` is created with Define, not Set.
let counter = 0;
Object.defineProperty(Array.prototype, "indices", {
  set() { counter++; }
});

let match = /a/.exec("a");
assert.sameValue(counter, 0);

// `indices` is a non-writable, non-enumerable, and configurable data-property.
verifyProperty(match, 'indices', {
  writable: true,
  enumerable: true,
  configurable: true
});
