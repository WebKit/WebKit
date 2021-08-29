// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.evaluate
description: >
  Realm.prototype.evaluate wrapped functions produce new wrapping on each evaluation.
features: [callable-boundary-realms]
---*/

assert.sameValue(
  typeof Realm.prototype.evaluate,
  'function',
  'This test must fail if Realm.prototype.evaluate is not a function'
);

const r = new Realm();

r.evaluate(`
function fn() {
    return 42;
}
`);

const wrapped = r.evaluate('fn');
const otherWrapped = r.evaluate('fn');

assert.notSameValue(wrapped, otherWrapped);
assert.sameValue(typeof wrapped, 'function');
assert.sameValue(typeof otherWrapped, 'function');
