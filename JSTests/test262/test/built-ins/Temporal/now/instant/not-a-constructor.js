// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.instant
description: Temporal.now.instant does not implement [[Construct]]
includes: [isConstructor.js]
features: [Reflect.construct, Temporal]
---*/

assert.sameValue(isConstructor(Temporal.now.instant), false, 'isConstructor(Temporal.now.instant) must return false');

assert.throws(TypeError, () => {
  new Temporal.now.instant();
}, '`new Temporal.now.instant()` throws TypeError');
