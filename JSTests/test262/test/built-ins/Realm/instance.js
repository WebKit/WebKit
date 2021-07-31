// Copyright (C) 2021 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-realm-constructor
description: >
  new Realm() returns a realm instance
info: |
  Realm ( )

  ...
  2. Let O be ? OrdinaryCreateFromConstructor(NewTarget, "%Realm.prototype%",
  « [[Realm]], [[ExecutionContext]] »).
  ...
  13. Return O.
features: [callable-boundary-realms]
---*/
assert.sameValue(
  typeof Realm,
  'function',
  'This test must fail if Realm is not a function'
);

var realm = new Realm();

assert(realm instanceof Realm);
assert.sameValue(
  Object.getPrototypeOf(realm),
  Realm.prototype,
  '[[Prototype]] is set to %Realm.prototype%'
);
