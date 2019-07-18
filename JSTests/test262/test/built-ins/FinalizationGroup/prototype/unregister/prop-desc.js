// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.unregister
description: >
  Property descriptor of FinalizationGroup.prototype.unregister
info: |
  17 ECMAScript Standard Built-in Objects:

  Every other data property described in clauses 18 through 26 and in Annex B.2
  has the attributes { [[Writable]]: true, [[Enumerable]]: false,
  [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
features: [FinalizationGroup]
---*/

assert.sameValue(typeof FinalizationGroup.prototype.unregister, 'function');

verifyProperty(FinalizationGroup.prototype, 'unregister', {
  enumerable: false,
  writable: true,
  configurable: true
});
