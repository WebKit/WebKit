// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.register
description: holdings may be the same as target
info: |
  FinalizationRegistry.prototype.register ( target , holdings [, unregisterToken ] )

  1. Let finalizationRegistry be the this value.
  2. If Type(finalizationRegistry) is not Object, throw a TypeError exception.
  3. If finalizationRegistry does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If Type(target) is not Object, throw a TypeError exception.
  5. If SameValue(target, holdings), throw a TypeError exception.
features: [FinalizationRegistry]
---*/

var finalizationRegistry = new FinalizationRegistry(function() {});

var target = {};
assert.throws(TypeError, () => finalizationRegistry.register(target, target));
