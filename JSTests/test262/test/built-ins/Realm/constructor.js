// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm-constructor
description: >
  Realm is a constructor and has [[Construct]] internal method.
includes: [isConstructor.js]
features: [callable-boundary-realms, Reflect.construct]
---*/
assert.sameValue(
  typeof Realm,
  'function',
  'This test must fail if Realm is not a function'
);

assert(isConstructor(Realm));
assert.sameValue(Object.getPrototypeOf(Realm), Function.prototype);
new Realm();
