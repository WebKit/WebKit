// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.evaluate
description: >
  Realm.prototype.evaluate wrapped functions share no properties
features: [callable-boundary-realms]
---*/

assert.sameValue(
  typeof Realm.prototype.evaluate,
  'function',
  'This test must fail if Realm.prototype.evaluate is not a function'
);

const r = new Realm();

const wrapped = r.evaluate(`
function fn() {
    return fn.secret;
}

fn.secret = 'confidential';
fn;
`);

assert.sameValue(wrapped.secret, undefined);
assert.sameValue(wrapped(), 'confidential');
