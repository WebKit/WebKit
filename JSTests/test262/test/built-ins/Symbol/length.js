// Copyright (C) 2017 Aleksey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-symbol-constructor
description: >
  Properties of the Symbol Constructor

  Besides the length property (whose value is 0)

includes: [propertyHelper.js]
features: [Symbol]
---*/

assert.sameValue(Symbol.length, 0, "The value of `Symbol.length` is `0`");

verifyNotEnumerable(Symbol, "length");
verifyNotWritable(Symbol, "length");
verifyConfigurable(Symbol, "length");
