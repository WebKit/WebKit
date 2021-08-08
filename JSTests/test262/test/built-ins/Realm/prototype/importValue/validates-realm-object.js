// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.importvalue
description: >
  Realm.prototype.importValue validates realm object.
features: [callable-boundary-realms]
---*/

assert.sameValue(
  typeof Realm.prototype.importValue,
  'function',
  'This test must fail if Realm.prototype.importValue is not a function'
);

const r = new Realm();
const bogus = {};

assert.throws(TypeError, function() {
  r.importValue.call(bogus, "specifier", "name");
}, 'throws a TypeError if this is not a Realm object');
