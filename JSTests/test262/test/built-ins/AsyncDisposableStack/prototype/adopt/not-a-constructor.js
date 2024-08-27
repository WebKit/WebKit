// Copyright (C) 2023 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-asyncdisposablestack.prototype.adopt
description: >
  AsyncDisposableStack.prototype.adopt does not implement [[Construct]], is not new-able
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
features: [Reflect.construct, explicit-resource-management, arrow-function]
---*/

assert.sameValue(
  isConstructor(AsyncDisposableStack.prototype.adopt),
  false,
  'isConstructor(AsyncDisposableStack.prototype.adopt) must return false'
);

assert.throws(TypeError, () => {
  let stack = new AsyncDisposableStack({}); new stack.adopt();
}, '`let stack = new AsyncDisposableStack({}); new stack.adopt()` throws TypeError');
