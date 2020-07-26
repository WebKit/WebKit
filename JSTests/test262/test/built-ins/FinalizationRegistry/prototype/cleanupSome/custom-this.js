// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.cleanupSome
description: Return values applying custom this
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationRegistry be the this value.
  2. If Type(finalizationRegistry) is not Object, throw a TypeError exception.
  3. If finalizationRegistry does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ! CleanupFinalizationRegistry(finalizationRegistry, callback).
  6. Return undefined.
features: [cleanupSome, FinalizationRegistry]
---*/

var fn = function() {};
var cleanupSome = FinalizationRegistry.prototype.cleanupSome;
var finalizationRegistry = new FinalizationRegistry(fn);

var cb = function() {};

assert.sameValue(cleanupSome.call(finalizationRegistry, cb), undefined);
assert.sameValue(cleanupSome.call(finalizationRegistry, fn), undefined), 'reuse the same cleanup callback fn';
