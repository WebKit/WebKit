// Copyright (C) 2018 Peter Wong. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: pending
description: IsRegExp should only be called once
info: |
  RegExp.prototype [ @@matchAll ] ( string )
    [...]
    3. Return ? MatchAllIterator(R, string).

  MatchAllIterator ( R, O )
    [...]
    2. If ? IsRegExp(R) is true, then
      [...]
    3. Else,
      a. Let flags be "g".
      b. Let matcher be ? RegExpCreate(R, flags).
features: [Symbol.match, Symbol.matchAll]
---*/

var internalCount = 0;
Object.defineProperty(RegExp.prototype, Symbol.match, {
  get: function() {
    ++internalCount;
    return true;
  }
});

var count = 0;
var o = {
  get [Symbol.match]() {
    ++count;
    return false;
  }
};

RegExp.prototype[Symbol.matchAll].call(o, '1');

assert.sameValue(0, internalCount);
assert.sameValue(1, count);
