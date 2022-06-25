// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-ecmascript-standard-built-in-objects
description: >
  FinalizationRegistry.prototype.register does not implement [[Construct]], is not new-able
info: |
  ECMAScript Function Objects

  Built-in function objects that are not identified as constructors do not
  implement the [[Construct]] internal method unless otherwise specified in
  the description of a particular function.

  sec-evaluatenew

  ...
  7. If IsConstructor(constructor) is false, throw a TypeError exception.
  ...
includes: [isConstructor.js]
features: [Reflect.construct, FinalizationRegistry, arrow-function]
---*/

assert.sameValue(
  isConstructor(FinalizationRegistry.prototype.register),
  false,
  'isConstructor(FinalizationRegistry.prototype.register) must return false'
);

assert.throws(TypeError, () => {
  let fr = new FinalizationRegistry(() => {}); new fr.register({});
}, '`let fr = new FinalizationRegistry(() => {}); new fr.register({})` throws TypeError');

