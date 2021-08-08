// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.evaluate
description: >
  Realm.prototype.evaluate is not a constructor.
includes: [isConstructor.js]
features: [callable-boundary-realms, Reflect.construct]
---*/

assert.sameValue(
  typeof Realm.prototype.evaluate,
  'function',
  'This test must fail if Realm.prototype.evaluate is not a function'
);

assert.sameValue(
  isConstructor(Realm.prototype.evaluate),
  false,
  'isConstructor(Realm.prototype.evaluate) must return false'
);

assert.throws(TypeError, () => {
  new Realm.prototype.evaluate("");
}, '`new Realm.prototype.evaluate("")` throws TypeError');

const r = new Realm(); 
r.evaluate('globalThis.x = 0');

assert.throws(TypeError, () => {
  new r.evaluate("globalThis.x += 1;");
}, '`new r.evaluate("...")` throws TypeError');

assert.sameValue(r.evaluate('globalThis.x'), 0, 'No code evaluated in the new expression');
