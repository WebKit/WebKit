// Copyright (C) 2020 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%asyncfromsynciteratorprototype%.throw
description: >
  `throw` method does not pass absent `value`.
info: |
  %AsyncFromSyncIteratorPrototype%.throw ( value )

  [...]
  8. If value is present, then
    [...]
  9. Else,
    a. Let result be Call(throw, syncIterator).
  [...]
flags: [async]
features: [async-iteration]
---*/

var throwArgumentsLength;
var syncIterator = {
  [Symbol.iterator]() {
    return this;
  },
  next() {
    return {done: false};
  },
  throw() {
    throwArgumentsLength = arguments.length;
    return {done: true};
  },
};

var asyncIterator = (async function* () {
  yield* syncIterator;
})();

asyncIterator.next().then(function() {
  return asyncIterator.throw();
}).then(function(result) {
  throw new Test262Error("Promise should be rejected, got: " + result.value);
}, function() {
  assert.sameValue(throwArgumentsLength, 0);
}).then($DONE, $DONE);
