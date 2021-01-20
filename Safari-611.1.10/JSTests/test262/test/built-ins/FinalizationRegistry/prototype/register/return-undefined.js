// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.register
description: Return undefined after registering
info: |
  FinalizationRegistry.prototype.register ( target , holdings [, unregisterToken ] )

  1. Let finalizationRegistry be the this value.
  2. If Type(finalizationRegistry) is not Object, throw a TypeError exception.
  3. If Type(target) is not Object, throw a TypeError exception.
  4. If finalizationRegistry does not have a [[Cells]] internal slot, throw a TypeError exception.
  5. If Type(unregisterToken) is not Object,
    a. If unregisterToken is not undefined, throw a TypeError exception.
    b. Set unregisterToken to empty.
  6. Let cell be the Record { [[Target]] : target, [[Holdings]]: holdings, [[UnregisterToken]]: unregisterToken }.
  7. Append cell to finalizationRegistry.[[Cells]].
  8. Return undefined.
features: [FinalizationRegistry]
---*/

var fn = function() {};
var finalizationRegistry = new FinalizationRegistry(fn);

var target = {};
assert.sameValue(finalizationRegistry.register(target), undefined, 'Register a targer');
assert.sameValue(finalizationRegistry.register(target), undefined, 'Register the same target again');
assert.sameValue(finalizationRegistry.register(target), undefined, 'Register the same target again and again');

assert.sameValue(finalizationRegistry.register({}), undefined, 'Register other targets');

assert.sameValue(finalizationRegistry.register(target, undefined, {}), undefined, 'Register target with unregisterToken');
assert.sameValue(
  finalizationRegistry.register(target, undefined, target),
  undefined,
  'Register target with unregisterToken being the registered target'
);

assert.sameValue(finalizationRegistry.register(target, undefined, undefined), undefined, 'Register target with explicit undefined unregisterToken');

assert.sameValue(finalizationRegistry.register(fn), undefined, 'register the cleanup callback fn');
