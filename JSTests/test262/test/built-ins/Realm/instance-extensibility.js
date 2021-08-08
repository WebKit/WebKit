// Copyright (C) 2021 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-realm
description: >
  The new instance is extensible
info: |
  Realm ( )

  ...
  2. Let O be ? OrdinaryCreateFromConstructor(NewTarget, "%Realm.prototype%",
  « [[Realm]], [[ExecutionContext]] »).
  ...
  13. Return O.

  OrdinaryCreateFromConstructor creates a new ordinary objects including the
  internal slots [[Prototype]] and [[Extensible]]. The latter will have its
  value set to true.
includes: [propertyHelper.js]
features: [callable-boundary-realms]
---*/

const realm = new Realm();

assert(Object.isExtensible(realm));

Object.defineProperty(realm, 'foo', {});
assert(realm.hasOwnProperty('foo'), 'confirms extensibility adding a new property');

Object.defineProperty(realm, 'foo', {
  value: 'bar',
  writable: true,
  configurable: true,
  enumerable: false,
});

verifyProperty(realm, 'foo', {
  value: 'bar',
  writable: true,
  configurable: true,
  enumerable: false,
});
