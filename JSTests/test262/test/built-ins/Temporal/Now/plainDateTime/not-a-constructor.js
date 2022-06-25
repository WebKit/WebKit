// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.plaindatetime
description: Temporal.Now.plainDateTime does not implement [[Construct]]
includes: [isConstructor.js]
features: [Reflect.construct, Temporal, arrow-function]
---*/

assert.sameValue(isConstructor(Temporal.Now.plainDateTime), false, 'isConstructor(Temporal.Now.plainDateTime) must return false');

assert.throws(TypeError, () => {
  new Temporal.Now.plainDateTime();
}, 'new Temporal.Now.plainDateTime() throws a TypeError exception');
