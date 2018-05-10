// Copyright (C) 2018 Peter Wong. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: pending
description: Custom species constructor is called when creating internal RegExp
info: |
  RegExp.prototype [ @@matchAll ] ( string )
    [...]
    3. Return ? MatchAllIterator(R, string).

  MatchAllIterator ( R, O )
    [...]
    2. If ? IsRegExp(R) is true, then
      a. Let C be ? SpeciesConstructor(R, RegExp).
      b. Let flags be ? ToString(? Get(R, "flags"))
      c. Let matcher be ? Construct(C, R, flags).
features: [Symbol.matchAll, Symbol.species]
includes: [compareArray.js, compareIterator.js, regExpUtils.js]
---*/

var callCount = 0;
var callArgs;
var regexp = /\d/u;
regexp.constructor = {
  [Symbol.species]: function(){
    callCount++;
    callArgs = arguments;
    return /\w/g;
  }
};
var str = 'a*b';
var iter = regexp[Symbol.matchAll](str);

assert.sameValue(callCount, 1);
assert.sameValue(callArgs.length, 2);
assert.sameValue(callArgs[0], regexp);
assert.sameValue(callArgs[1], 'u');

assert.compareIterator(iter, [
  matchValidator(['a'], 0, str),
  matchValidator(['b'], 2, str)
]);
