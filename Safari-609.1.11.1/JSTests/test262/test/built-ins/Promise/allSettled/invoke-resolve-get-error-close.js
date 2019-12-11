// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Error retrieving the constructor's `resolve` method not opening iterator
esid: sec-promise.allsettled
info: |
  6. Let result be PerformPromiseAllSettled(iteratorRecord, C, promiseCapability).
  7. If result is an abrupt completion, then
    a. If iteratorRecord.[[Done]] is false, set result to IteratorClose(iteratorRecord, result).
    b. IfAbruptRejectPromise(result, promiseCapability).

  Runtime Semantics: PerformPromiseAllSettled

  6. Repeat
    ...
    i. Let nextPromise be ? Invoke(constructor, "resolve", « nextValue »).
features: [Promise.allSettled, Symbol.iterator]
---*/

var iter = {};
var returnCount = 0;
var nextCount = 0;
iter[Symbol.iterator] = function() {
  return {
    next() {
      nextCount += 1;
      return {
        done: false
      };
    },
    return() {
      returnCount += 1;
      return {};
    }
  };
};
Object.defineProperty(Promise, 'resolve', {
  get() {
    throw new Test262Error();
  }
});

Promise.allSettled(iter);

assert.sameValue(nextCount, 0);
assert.sameValue(returnCount, 1);
