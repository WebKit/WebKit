// Copyright (C) 2021 Microsoft. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.findlast
description: >
  Predicate thisArg as F.call( thisArg, kValue, k, O ) for each array entry.
info: |
  Array.prototype.findLast ( predicate[ , thisArg ] )

  ...
  5. Repeat, while k ≥ 0,
    ...
    c. Let testResult be ! ToBoolean(? Call(predicate, thisArg, « kValue, 𝔽(k), O »)).
    d. If testResult is true, return kValue.
  ...
flags: [noStrict]
features: [array-find-from-last]
---*/

var result;

[1].findLast(function() {
  result = this;
});

assert.sameValue(result, this);

var o = {};
[1].findLast(function() {
  result = this;
}, o);

assert.sameValue(result, o);
