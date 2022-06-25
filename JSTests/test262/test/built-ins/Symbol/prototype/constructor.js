// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-symbol.prototype.constructor
description: Property descriptor
info: |
    Every other data property described in clauses 18 through 26 and in Annex
    B.2 has the attributes { [[Writable]]: true, [[Enumerable]]: false,
    [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
features: [Symbol]
---*/

assert.sameValue(Symbol.prototype.constructor, Symbol);

verifyNotEnumerable(Symbol.prototype, 'constructor');
verifyWritable(Symbol.prototype, 'constructor');
verifyConfigurable(Symbol.prototype, 'constructor');
