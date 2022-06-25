// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: Resolving with a non-thenable object value from a pending promise that is later fulfilled
es6id: 25.4.5.3
info: |
    [...]
    7. Return PerformPromiseThen(promise, onFulfilled, onRejected,
       resultCapability).

    25.4.5.3.1 PerformPromiseThen
    [...]
    7. If the value of promise's [[PromiseState]] internal slot is "pending",
       a. Append fulfillReaction as the last element of the List that is the
          value of promise's [[PromiseFulfillReactions]] internal slot.
       [...]

    25.4.1.3.2 Promise Resolve Functions
    [...]
    8. Let then be Get(resolution, "then").
    9. If then is an abrupt completion, then
       [...]
    10. Let thenAction be then.[[value]].
    11. If IsCallable(thenAction) is false, then
        a. Return FulfillPromise(promise, resolution).
flags: [async]
---*/

var nonThenable = {
  then: null
};
var resolve;
var p1 = new Promise(function(_resolve) {
  resolve = _resolve;
});
var p2;

p2 = p1.then(function() {
  return nonThenable;
});

p2.then(function(value) {
  if (value !== nonThenable) {
    $DONE('The promise should be fulfilled with the provided value.');
    return;
  }

  $DONE();
}, function() {
  $DONE('The promise should not be rejected.');
});

resolve();
