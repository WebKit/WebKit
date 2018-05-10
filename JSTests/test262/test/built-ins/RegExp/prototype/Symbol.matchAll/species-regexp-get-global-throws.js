// Copyright (C) 2018 Peter Wong. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: pending
description: |
  Re-throws errors thrown while accessing species constructed RegExp's
  global property
info: |
  RegExp.prototype [ @@matchAll ] ( string )
    [...]
    3. Return ? MatchAllIterator(R, string).

  MatchAllIterator ( R, O )
    [...]
    2. If ? IsRegExp(R) is true, then
      [...]
      d. Let global be ? ToBoolean(? Get(matcher, "global")).
features: [Symbol.matchAll, Symbol.species]
---*/

var regexp = /./;
regexp.constructor = {
  [Symbol.species]: function() {
    return Object.defineProperty(/./, 'global', {
      get() {
        throw new Test262Error();
      }
    });
  }
};

assert.throws(Test262Error, function() {
  regexp[Symbol.matchAll]('');
});
