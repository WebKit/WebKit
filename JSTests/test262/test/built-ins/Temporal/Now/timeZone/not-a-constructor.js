// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.timezone
description: Temporal.Now.timeZone does not implement [[Construct]]
info: |
  ECMAScript Function Objects

  Built-in function objects that are not identified as constructors do not
  implement the [[Construct]] internal method unless otherwise specified in
  the description of a particular function.
includes: [isConstructor.js]
features: [Reflect.construct, Temporal, arrow-function]
---*/

assert.sameValue(isConstructor(Temporal.Now.timeZone), false, 'isConstructor(Temporal.Now.timeZone) must return false');

assert.throws(TypeError, () => {
  new Temporal.Now.timeZone();
}, 'new Temporal.Now.timeZone() throws a TypeError exception');
