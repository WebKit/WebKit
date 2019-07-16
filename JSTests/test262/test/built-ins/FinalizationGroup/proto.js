// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-group-constructor
description: >
  The prototype of FinalizationGroup is Object.prototype
info: |
  The value of the [[Prototype]] internal slot of the FinalizationGroup object is the
  intrinsic object %FunctionPrototype%.
features: [FinalizationGroup]
---*/

assert.sameValue(
  Object.getPrototypeOf(FinalizationGroup),
  Function.prototype,
  'Object.getPrototypeOf(FinalizationGroup) returns the value of `Function.prototype`'
);
