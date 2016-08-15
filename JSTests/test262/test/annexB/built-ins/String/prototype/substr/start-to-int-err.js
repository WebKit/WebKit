// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-string.prototype.substr
es6id: B.2.3.1
description: >
    Behavior when "start" integer conversion triggers an abrupt completion
info: |
    [...]
    3. Let intStart be ? ToInteger(start).
features: [Symbol]
---*/

var lengthCallCount = 0;
var symbol = Symbol('');
var start = {
  valueOf: function() {
    throw new Test262Error();
  }
};
var length = {
  valueOf: function() {
    lengthCallCount += 1;
  }
};

assert.throws(Test262Error, function() {
  ''.substr(start, length);
});

assert.throws(TypeError, function() {
  ''.substr(symbol, length);
});

assert.sameValue(lengthCallCount, 0);
