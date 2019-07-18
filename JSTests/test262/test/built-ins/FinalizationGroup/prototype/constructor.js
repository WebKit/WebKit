// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.constructor
description: FinalizationGroup.prototype.constructor property descriptor
info: |
  FinalizationGroup.prototype.constructor

  The initial value of FinalizationGroup.prototype.constructor is the intrinsic
  object %FinalizationGroup%.

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
features: [FinalizationGroup]
---*/

verifyProperty(FinalizationGroup.prototype, 'constructor', {
  value: FinalizationGroup,
  writable: true,
  enumerable: false,
  configurable: true
});
