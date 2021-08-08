// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.evaluate
description: >
  Realm.prototype.evaluate wrapped function arguments are wrapped into the inner realm
features: [callable-boundary-realms]
---*/

assert.sameValue(
  typeof Realm.prototype.evaluate,
  'function',
  'This test must fail if Realm.prototype.evaluate is not a function'
);

const r = new Realm();
const blueFn = (x, y) => x + y;

const redWrappedFn = r.evaluate(`
0, (blueWrappedFn, a, b, c) => {
    return blueWrappedFn(a, b) * c;
}
`);
assert.sameValue(redWrappedFn(blueFn, 2, 3, 4), 20);
