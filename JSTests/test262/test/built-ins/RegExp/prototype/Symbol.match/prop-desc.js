// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 21.2.5.6
description: RegExp.prototype[Symbol.match] property descriptor
info: |
    ES6 Section 17

    Every other data property described in clauses 18 through 26 and in Annex
    B.2 has the attributes { [[Writable]]: true, [[Enumerable]]: false,
    [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
features: [Symbol.match]
---*/

verifyNotEnumerable(RegExp.prototype, Symbol.match);
verifyWritable(RegExp.prototype, Symbol.match);
verifyConfigurable(RegExp.prototype, Symbol.match);
