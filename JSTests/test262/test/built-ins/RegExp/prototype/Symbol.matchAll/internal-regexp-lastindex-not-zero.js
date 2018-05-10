// Copyright (C) 2018 Peter Wong. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: pending
description: |
  Throws TypeError when internally created RegExp's lastIndex is not 0
info: |
  RegExp.prototype [ @@matchAll ] ( string )
    [...]
    3. Return ? MatchAllIterator(R, string).

  MatchAllIterator ( R, O )
    [...]
    2. If ? IsRegExp(R) is true, then
      [...]
    3. Else,
      a. Let matcher be RegExpCreate(R, "g").
      b. If ? IsRegExp(matcher) is not true, throw a TypeError exception.
      [...]
      3. If Get(matcher, "lastIndex") is not 0, throw a TypeError exception.
features: [Symbol.match, Symbol.matchAll]
---*/

Object.defineProperty(RegExp.prototype, Symbol.match, {
  get() {
    this.lastIndex = 1;
    return true;
  }
});

assert.throws(TypeError, function() {
  RegExp.prototype[Symbol.matchAll].call({}, '');
});
