// Copyright (C) 2018 Valerie Young. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%asyncfromsynciteratorprototype%.throw
description: throw() will return rejected promise if sync `throw` undefined
info: |
  %AsyncFromSyncIteratorPrototype%.throw ( value )
  ...
  2. Let promiseCapability be ! NewPromiseCapability(%Promise%).
  ...
  5. Let return be GetMethod(syncIterator, "throw").
  6. IfAbruptRejectPromise(throw, promiseCapability).
  7. If throw is undefined, then
    a. Perform ! Call(promiseCapability.[[Reject]], undefined, « value »).
    b. Return promiseCapability.[[Promise]].

flags: [async]
features: [async-iteration]
---*/

var obj = {
  [Symbol.iterator]() {
    return {
      next() {
        return { value: 1, done: false };
      }
    };
  }
};

async function* asyncg() {
  yield* obj;
}

var iter = asyncg();

iter.next().then(function(result) {
  iter.throw().then(
    function (result) {
      throw new Test262Error("Promise should be rejected, got: " + result.value);
    },
    function (err) {
      assert.sameValue(err, undefined, "Promise should be rejected with undefined");

      iter.next().then(({ done, value }) => {
        assert.sameValue(done, true, 'the iterator is completed');
        assert.sameValue(value, undefined, 'value is undefined');
      }).then($DONE, $DONE);
    }
  ).catch($DONE);

}).catch($DONE);
