// Copyright (C) 2021 Microsoft. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-ecmascript-standard-built-in-objects
description: >
  Array.prototype.findLast does not implement [[Construct]], is not new-able.
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
features: [Reflect.construct, arrow-function, array-find-from-last]
---*/

assert.sameValue(isConstructor(Array.prototype.findLast), false, 'isConstructor(Array.prototype.findLast) must return false');

assert.throws(TypeError, () => {
  new Array.prototype.findLast(() => {});
}, '`new Array.prototype.findLast(() => {})` throws TypeError');

