// Copyright (C) 2023 Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.1.2.1
description: Property descriptor
info: |
    ES6 Section 17

    Every other data property described in clauses 18 through 26 and in Annex
    B.2 has the attributes { [[Writable]]: true, [[Enumerable]]: false,
    [[Configurable]]: true } unless otherwise specified.
features: [iterator-helpers]
includes: [propertyHelper.js]
---*/
verifyProperty(Iterator.prototype, Symbol.toStringTag, {
  value: 'Iterator',
  writable: true,
  enumerable: false,
  configurable: true,
});
