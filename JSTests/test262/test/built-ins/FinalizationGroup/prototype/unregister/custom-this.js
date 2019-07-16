// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.unregister
description: Return values applying custom this
info: |
  FinalizationGroup.prototype.unregister ( unregisterToken )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If Type(unregisterToken) is not Object, throw a TypeError exception.
  5. Let removed be false.
  6. For each Record { [[Target]], [[Holdings]], [[UnregisterToken]] } cell that is an element of finalizationGroup.[[Cells]], do
    a. If SameValue(cell.[[UnregisterToken]], unregisterToken) is true, then
      i. Remove cell from finalizationGroup.[[Cells]].
      ii. Set removed to true.
  7. Return removed.
features: [FinalizationGroup]
---*/

var fn = function() {};
var unregister = FinalizationGroup.prototype.unregister;
var fg = new FinalizationGroup(fn);

var target1 = {};
var target2 = {};
var target3 = {};
var token1 = {};
var token2 = {};

assert.sameValue(unregister.call(fg, token1), false, 'unregistering token1 from empty fg');
assert.sameValue(unregister.call(fg, token2), false, 'unregistering token2 from empty fg');

fg.register(target1, undefined, token1);
fg.register(target2, undefined, token2);
fg.register(target3, undefined, token2);

assert.sameValue(unregister.call(fg, target1), false, 'own target does not work on unregister, #1');
assert.sameValue(unregister.call(fg, target2), false, 'own target does not work on unregister, #2');
assert.sameValue(unregister.call(fg, target3), false, 'own target does not work on unregister, #3');

assert.sameValue(unregister.call(fg, token1), true, 'unregistering token1 from fg');
assert.sameValue(unregister.call(fg, token1), false, 'unregistering token1 again from fg');
assert.sameValue(unregister.call(fg, token2), true, 'unregistering token2 to remove target2 and target3');
assert.sameValue(unregister.call(fg, token2), false, 'unregistering token2 from empty fg');
