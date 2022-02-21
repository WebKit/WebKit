// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.zoneddatetime
description: Temporal.Now.zonedDateTime does not implement [[Construct]]
includes: [isConstructor.js]
features: [Reflect.construct, Temporal, arrow-function]
---*/

assert.sameValue(isConstructor(Temporal.Now.zonedDateTime), false, 'isConstructor(Temporal.Now.zonedDateTime) must return false');

assert.throws(TypeError, () => {
  new Temporal.Now.zonedDateTime();
}, 'new Temporal.Now.zonedDateTime() throws a TypeError exception');
