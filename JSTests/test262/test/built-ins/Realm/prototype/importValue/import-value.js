// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.importvalue
description: >
  Realm.prototype.importValue can import a value.
flags: [async, module]
features: [callable-boundary-realms]
---*/

assert.sameValue(
  typeof Realm.prototype.importValue,
  'function',
  'This test must fail if Realm.prototype.importValue is not a function'
);

const r = new Realm();

r.importValue('./import-value_FIXTURE.js', 'x').then(x => {

  assert.sameValue(x, 1);

}).then($DONE, $DONE);
