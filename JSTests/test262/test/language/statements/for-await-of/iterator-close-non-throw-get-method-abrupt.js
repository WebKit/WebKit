// Copyright (C) 2020 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-asynciteratorclose
description: >
  If retrieving an iterator's `return` method generates an error while
  closing the iterator with non-throw completion, the error should be
  forwarded to the runtime.
info: |
  AsyncIteratorClose ( iteratorRecord, completion )

  [...]
  4. Let innerResult be GetMethod(iterator, "return").
  5. If innerResult.[[Type]] is normal,
    [...]
  6. If completion.[[Type]] is throw, return Completion(completion).
  7. If innerResult.[[Type]] is throw, return Completion(innerResult).

  GetMethod ( V, P )

  [...]
  2. Let func be ? GetV(V, P).
features: [async-iteration]
flags: [async]
---*/

const innerError = { name: "inner error" };
const asyncIterable = {};
asyncIterable[Symbol.asyncIterator] = function() {
  return {
    next: function() {
      return { done: false, value: null };
    },
    get return() {
      throw innerError;
    },
  };
};

let iterationCount = 0;
const promise = (async function() {
  for await (const x of asyncIterable) {
    iterationCount += 1;
    break;
  }
})();

promise.then(function(value) {
  throw new Test262Error("Promise should be rejected, got: " + value);
}, function(error) {
  assert.sameValue(error, innerError);
  assert.sameValue(iterationCount, 1, "The loop body is evaluated");
}).then($DONE, $DONE);
