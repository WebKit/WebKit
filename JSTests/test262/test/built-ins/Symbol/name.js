// Copyright (C) 2017 Aleksey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-symbol-constructor
description: >
  Symbol ( [ description ] )

includes: [propertyHelper.js]
features: [Symbol]
---*/

assert.sameValue(Symbol.name, "Symbol", "The value of `Symbol.name` is `'Symbol'`");

verifyNotEnumerable(Symbol, "name");
verifyNotWritable(Symbol, "name");
verifyConfigurable(Symbol, "name");
