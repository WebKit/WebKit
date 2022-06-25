// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-string.prototype.replaceall
description: >
  Returns abrupt completions from ToString(this value)
info: |
  String.prototype.replaceAll ( searchValue, replaceValue )

  1. Let O be RequireObjectCoercible(this value).
  2. If searchValue is neither undefined nor null, then
    ...
  3. Let string be ? ToString(O).
  ...
features: [String.prototype.replaceAll, Symbol]
---*/

assert.sameValue(
  typeof String.prototype.replaceAll,
  'function',
  'function must exist'
);

var poisoned = 0;
var poison = {
  toString() {
    poisoned += 1;
    throw 'Should not call toString on replaceValue';
  },
};

var called = 0;
var thisValue = {
  toString() {
    called += 1;
    throw new Test262Error();
  }
}

var searchValue = {
  toString() {
    throw 'Should not call toString on searchValue';
  }
};

assert.throws(Test262Error, function() {
  ''.replaceAll.call(thisValue, searchValue, poison);
}, 'custom');
assert.sameValue(called, 1);

called = 0;
thisValue = {
  toString() {
    called += 1;
    return Symbol();
  }
};

assert.throws(TypeError, function() {
  ''.replaceAll.call(thisValue, searchValue, poison);
}, 'Symbol');
assert.sameValue(called, 1);

assert.sameValue(poisoned, 0);
