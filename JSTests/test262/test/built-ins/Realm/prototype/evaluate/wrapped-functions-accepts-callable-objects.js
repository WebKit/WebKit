// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm.prototype.evaluate
description: >
  Realm.prototype.evaluate accepts callable objects
features: [callable-boundary-realms]
---*/

assert.sameValue(
  typeof Realm.prototype.evaluate,
  'function',
  'This test must fail if Realm.prototype.evaluate is not a function'
);

const r = new Realm();

assert.sameValue(typeof r.evaluate('function fn() {} fn'), 'function', 'value from a fn declaration');
assert.sameValue(typeof r.evaluate('(function() {})'), 'function', 'function expression');
assert.sameValue(typeof r.evaluate('(async function() {})'), 'function', 'async function expression');
assert.sameValue(typeof r.evaluate('(function*() {})'), 'function', 'generator expression');
assert.sameValue(typeof r.evaluate('(async function*() {})'), 'function', 'async generator expression');
assert.sameValue(typeof r.evaluate('() => {}'), 'function', 'arrow function');
