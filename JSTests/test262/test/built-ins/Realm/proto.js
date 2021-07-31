// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-properties-of-the-realm-constructor
description: >
  The [[Prototype]] of Realm is Function.Prototype.
info: |
  Unless otherwise specified every built-in function and every built-in constructor
  has the Function prototype object, which is the initial value of the expression
  Function.prototype, as the value of its [[Prototype]] internal slot.

features: [callable-boundary-realms]
---*/

assert.sameValue(Object.getPrototypeOf(Realm), Function.prototype);
