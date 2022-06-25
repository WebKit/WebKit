// Copyright (C) 2018 Peter Wong. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: pending
description: Re-throws errors thrown while accessing RegExp's flags property
info: |
  RegExp.prototype [ @@matchAll ] ( string )
    [...]
    3. Return ? MatchAllIterator(R, string).

  MatchAllIterator ( R, O )
    [...]
    2. If ? IsRegExp(R) is true, then
      [...]
      b. Let flags be ? ToString(? Get(R, "flags"))
features: [Symbol.matchAll]
---*/

var regexp = /./;
Object.defineProperty(regexp, 'flags', {
  get() {
    throw new Test262Error();
  }
});

assert.throws(Test262Error, function() {
  regexp[Symbol.matchAll]('');
});
