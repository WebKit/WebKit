// Copyright (C) 2020 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%asyncfromsynciteratorprototype%.return
description: >
  `return` method does not pass absent `value`.
info: |
  %AsyncFromSyncIteratorPrototype%.return ( value )

  [...]
  8. If value is present, then
    [...]
  9. Else,
    a. Let result be Call(return, syncIterator).
  [...]
flags: [async]
features: [async-iteration]
---*/

var returnArgumentsLength;
var syncIterator = {
  [Symbol.iterator]() {
    return this;
  },
  next() {
    return {done: false};
  },
  return() {
    returnArgumentsLength = arguments.length;
    return {done: true};
  },
};

var asyncIterator = (async function* () {
  yield* syncIterator;
})();

asyncIterator.next().then(function() {
  return asyncIterator.return();
}).then(function() {
  assert.sameValue(returnArgumentsLength, 0);
}).then($DONE, $DONE);
