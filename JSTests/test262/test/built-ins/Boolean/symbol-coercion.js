// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-toboolean
es6id: 7.1.2
description: >
    Boolean coercion operations on Symbols
features: [Symbol]
---*/
var sym = Symbol();

assert.sameValue(Boolean(sym).valueOf(), true, "`Boolean(sym).valueOf()` returns `true`");
