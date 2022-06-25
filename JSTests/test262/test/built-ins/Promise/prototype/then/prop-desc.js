// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.4.5.1
description: Promise.prototype.then property descriptor
info: |
    Every other data property described in clauses 18 through 26 and in Annex
    B.2 has the attributes { [[Writable]]: true, [[Enumerable]]: false,
    [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
---*/

assert.sameValue(typeof Promise.prototype.then, 'function');

verifyNotEnumerable(Promise.prototype, 'then');
verifyWritable(Promise.prototype, 'then');
verifyConfigurable(Promise.prototype, 'then');
