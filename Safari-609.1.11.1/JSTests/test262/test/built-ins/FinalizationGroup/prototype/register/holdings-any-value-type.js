// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.register
description: No restriction for the value or type of holdings
info: |
  FinalizationGroup.prototype.register ( target , holdings [, unregisterToken ] )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If Type(target) is not Object, throw a TypeError exception.
  4. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  5. If Type(unregisterToken) is not Object,
    a. If unregisterToken is not undefined, throw a TypeError exception.
    b. Set unregisterToken to empty.
  6. Let cell be the Record { [[Target]] : target, [[Holdings]]: holdings, [[UnregisterToken]]: unregisterToken }.
  7. Append cell to finalizationGroup.[[Cells]].
  8. Return undefined.
features: [FinalizationGroup]
---*/

var fn = function() {};
var fg = new FinalizationGroup(fn);

var target = {};
assert.sameValue(fg.register(target, undefined), undefined, 'undefined');
assert.sameValue(fg.register(target, null), undefined, 'null');
assert.sameValue(fg.register(target, false), undefined, 'false');
assert.sameValue(fg.register(target, true), undefined, 'true');
assert.sameValue(fg.register(target, Symbol()), undefined, 'symbol');
assert.sameValue(fg.register(target, {}), undefined, 'object');
assert.sameValue(fg.register(target, fg), undefined, 'same as fg instance');
assert.sameValue(fg.register(target, 1), undefined, 'number');
assert.sameValue(fg.register(target, 'holdings'), undefined, 'string');
