// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.importvalue
description: >
  Realm.prototype.importValue coerces exportName to string.
features: [callable-boundary-realms]
---*/

assert.sameValue(
  typeof Realm.prototype.importValue,
  'function',
  'This test must fail if Realm.prototype.importValue is not a function'
);

const r = new Realm();
let count = 0;

const exportName = {
  toString() {
    count += 1;
    throw new Test262Error();
  }
};

assert.throws(Test262Error, () => {
  r.importValue('', exportName);
});

assert.sameValue(count, 1);
