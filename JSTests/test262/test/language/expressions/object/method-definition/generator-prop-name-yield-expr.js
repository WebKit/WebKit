// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When the `yield` keyword occurs within the PropertyName of a
    GeneratorMethod within a generator function, it behaves as a
    YieldExpression.
es6id: 14.4
features: [generators]
flags: [noStrict]
---*/

var obj = null;
var yield = 'propNameViaIdentifier';
var iter = (function*() {
  obj = {
    *[yield]() {}
  };
})();

iter.next();

assert.sameValue(obj, null);

iter.next('propNameViaExpression');

assert.sameValue(
  Object.hasOwnProperty.call(obj, 'propNameViaIdentifier'), false
);
assert.sameValue(
  Object.hasOwnProperty.call(obj, 'propNameViaExpression'), true
);
