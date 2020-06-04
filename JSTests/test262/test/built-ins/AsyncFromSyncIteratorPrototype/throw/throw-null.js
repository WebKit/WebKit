// Copyright (C) 2020 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%asyncfromsynciteratorprototype%.throw
description: >
  If syncIterator's "throw" method is `null`,
  a Promise rejected with provided value is returned.
info: |
  %AsyncFromSyncIteratorPrototype%.throw ( value )

  [...]
  5. Let throw be GetMethod(syncIterator, "throw").
  [...]
  7. If throw is undefined, then
    a. Perform ! Call(promiseCapability.[[Reject]], undefined, « value »).
    b. Return promiseCapability.[[Promise]].

  GetMethod ( V, P )

  [...]
  2. Let func be ? GetV(V, P).
  3. If func is either undefined or null, return undefined.
flags: [async]
features: [async-iteration]
---*/

var throwGets = 0;
var syncIterator = {
  [Symbol.iterator]() {
    return this;
  },
  next() {
    return {value: 1, done: false};
  },
  get throw() {
    throwGets += 1;
    return null;
  },
};

async function* asyncGenerator() {
  yield* syncIterator;
}

var asyncIterator = asyncGenerator();
var thrownError = { name: "err" };

asyncIterator.next().then(function() {
  return asyncIterator.throw(thrownError);
}).then(function(result) {
  throw new Test262Error("Promise should be rejected, got: " + result.value);
}, function(err) {
  assert.sameValue(err, thrownError);
  return asyncIterator.next().then(function(result) {
    assert.sameValue(result.value, undefined);
    assert.sameValue(result.done, true);
  });
}).then($DONE, $DONE);
