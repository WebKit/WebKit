// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When the `yield` keyword occurs within the PropertyName of a
    GeneratorMethod outside of a generator function, it behaves as an
    Identifier.
es6id: 14.4
features: [generators]
flags: [noStrict]
---*/

var yield = 'propName';
var obj = {
  *[yield]() {}
};

assert.sameValue(Object.hasOwnProperty.call(obj, 'propName'), true);
