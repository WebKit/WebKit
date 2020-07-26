// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.cleanupSome
description: >
  Property descriptor of FinalizationRegistry.prototype.cleanupSome
info: |
  17 ECMAScript Standard Built-in Objects:

  Every other data property described in clauses 18 through 26 and in Annex B.2
  has the attributes { [[Writable]]: true, [[Enumerable]]: false,
  [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
features: [cleanupSome, FinalizationRegistry]
---*/

assert.sameValue(typeof FinalizationRegistry.prototype.cleanupSome, 'function');

verifyProperty(FinalizationRegistry.prototype, 'cleanupSome', {
  enumerable: false,
  writable: true,
  configurable: true
});
