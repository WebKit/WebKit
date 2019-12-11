// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: The prototype of FinalizationGroup.prototype is Object.prototype
esid: sec-properties-of-the-finalization-group-prototype-object
info: |
  The value of the [[Prototype]] internal slot of the FinalizationGroup prototype object
  is the intrinsic object %ObjectPrototype%.
features: [FinalizationGroup]
---*/

var proto = Object.getPrototypeOf(FinalizationGroup.prototype);
assert.sameValue(proto, Object.prototype);
