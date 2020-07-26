// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.cleanupSome
description: Return undefined regardless the result of CleanupFinalizationRegistry
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationRegistry be the this value.
  2. If Type(finalizationRegistry) is not Object, throw a TypeError exception.
  3. If finalizationRegistry does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ? CleanupFinalizationRegistry(finalizationRegistry, callback).
  6. Return undefined.
features: [cleanupSome, FinalizationRegistry, arrow-function, async-functions, async-iteration, class]
---*/

var fn = function() {};
var cb = function() {};
var poisoned = function() { throw new Test262Error(); };
var finalizationRegistry = new FinalizationRegistry(fn);

assert.sameValue(finalizationRegistry.cleanupSome(cb), undefined, 'regular callback');
assert.sameValue(finalizationRegistry.cleanupSome(fn), undefined, 'regular callback, same FG cleanup function');

assert.sameValue(finalizationRegistry.cleanupSome(() => {}), undefined, 'arrow function');
assert.sameValue(finalizationRegistry.cleanupSome(finalizationRegistry.cleanupSome), undefined, 'cleanupSome itself');
assert.sameValue(finalizationRegistry.cleanupSome(poisoned), undefined, 'poisoned');
assert.sameValue(finalizationRegistry.cleanupSome(class {}), undefined, 'class expression');
assert.sameValue(finalizationRegistry.cleanupSome(async function() {}), undefined, 'async function');
assert.sameValue(finalizationRegistry.cleanupSome(function *() {}), undefined, 'generator');
assert.sameValue(finalizationRegistry.cleanupSome(async function *() {}), undefined, 'async generator');

assert.sameValue(finalizationRegistry.cleanupSome(), undefined, 'undefined, implicit');
assert.sameValue(finalizationRegistry.cleanupSome(undefined), undefined, 'undefined, explicit');

var poisonedFg = new FinalizationRegistry(poisoned);

assert.sameValue(poisonedFg.cleanupSome(cb), undefined, 'regular callback on poisoned FG cleanup callback');
assert.sameValue(poisonedFg.cleanupSome(poisoned), undefined, 'poisoned callback on poisoned FG cleanup callback');
