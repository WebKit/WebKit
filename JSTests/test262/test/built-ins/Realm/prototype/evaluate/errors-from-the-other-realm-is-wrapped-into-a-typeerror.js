// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.evaluate
description: >
  Realm.prototype.evaluate wraps errors from other realm into TypeErrors
features: [callable-boundary-realms]
---*/

assert.sameValue(
  typeof Realm.prototype.evaluate,
  'function',
  'This test must fail if Realm.prototype.evaluate is not a function'
);

const r = new Realm();

assert.throws(TypeError, () => r.evaluate('...'), 'SyntaxError => TypeError');
assert.throws(TypeError, () => r.evaluate('throw 42'), 'throw primitive => TypeError');
assert.throws(TypeError, () => r.evaluate('throw new ReferenceError("aaa")'), 'custom ctor => TypeError');
assert.throws(TypeError, () => r.evaluate('throw new TypeError("aaa")'), 'Child TypeError => Parent TypeError');
