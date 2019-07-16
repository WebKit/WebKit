// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: The property descriptor FinalizationGroup.prototype
esid: sec-finalization-group.prototype
info: |
  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false,
  [[Configurable]]: false }.
features: [FinalizationGroup]
includes: [propertyHelper.js]
---*/

verifyProperty(FinalizationGroup, 'prototype', {
  writable: false,
  enumerable: false,
  configurable: false
});
