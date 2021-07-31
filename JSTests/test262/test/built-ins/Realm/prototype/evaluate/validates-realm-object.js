// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.evaluate
description: >
  Realm.prototype.evaluate validates realm object.
features: [callable-boundary-realms]
---*/

assert.sameValue(
  typeof Realm.prototype.evaluate,
  'function',
  'This test must fail if Realm.prototype.evaluate is not a function'
);

const r = new Realm();
const bogus = {};

assert.throws(TypeError, function() {
  r.evaluate.call(bogus, '');
}, 'throws a TypeError if this is not a Realm object');
