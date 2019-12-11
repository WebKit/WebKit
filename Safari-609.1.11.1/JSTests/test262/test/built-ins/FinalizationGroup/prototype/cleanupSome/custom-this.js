// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.cleanupSome
description: Return values applying custom this
info: |
  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ! CleanupFinalizationGroup(finalizationGroup, callback).
  6. Return undefined.
features: [FinalizationGroup]
---*/

var fn = function() {};
var cleanupSome = FinalizationGroup.prototype.cleanupSome;
var fg = new FinalizationGroup(fn);

var cb = function() {};

assert.sameValue(cleanupSome.call(fg, cb), undefined);
assert.sameValue(cleanupSome.call(fg, fn), undefined), 'reuse the same cleanup callback fn';
