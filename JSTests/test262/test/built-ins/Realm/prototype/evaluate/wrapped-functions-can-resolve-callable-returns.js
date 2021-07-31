// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.evaluate
description: >
  Realm.prototype.evaluate wrapped functions can resolve callable returns.
features: [callable-boundary-realms]
---*/

assert.sameValue(
  typeof Realm.prototype.evaluate,
  'function',
  'This test must fail if Realm.prototype.evaluate is not a function'
);

const r = new Realm();

const wrapped = r.evaluate('x => y => x * y');
const nestedWrapped = wrapped(2);
const otherNestedWrapped = wrapped(4);

assert.sameValue(otherNestedWrapped(3), 12);
assert.sameValue(nestedWrapped(3), 6);

assert.notSameValue(nestedWrapped, otherNestedWrapped, 'new wrapping for each return');
