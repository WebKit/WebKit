// Copyright (C) 2018 Peter Wong. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: pending
description: Behavior when @@matchAll is removed from RegExp's prototype
info: |
  String.prototype.matchAll ( regexp )
    1. Let O be ? RequireObjectCoercible(this value).
    2. If regexp is neither undefined nor null, then
      a. Let matcher be ? GetMethod(regexp, @@matchAll).
      b. If matcher is not undefined, then
        [...]
    3. Return ? MatchAllIterator(regexp, O).
features: [Symbol.matchAll, String.prototype.matchAll]
includes: [compareArray.js, compareIterator.js, regExpUtils.js]
---*/

delete RegExp.prototype[Symbol.matchAll];
var str = 'a*b';

assert.compareIterator(str.matchAll(/\w/g), [
  matchValidator(['a'], 0, str),
  matchValidator(['b'], 2, str)
]);
