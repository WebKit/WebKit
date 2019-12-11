// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Error retrieving the constructor's `resolve` method before iterator being used.
esid: sec-performpromiseall
info: |
    11. Let result be PerformPromiseAll(iteratorRecord, C, promiseCapability).
    12. If result is an abrupt completion,
        a. If iteratorRecord.[[done]] is false, let result be
           IteratorClose(iterator, result).
        b. IfAbruptRejectPromise(result, promiseCapability).

    ...

    Runtime Semantics: PerformPromiseAll

    ...
    1. Let promiseResolve be ? Get(constructor, `"resolve"`).
    ...
    1. Repeat,
      1. Let next be IteratorStep(iteratorRecord).
      ...
      1. Let nextPromise be ? Call(promiseResolve, constructor, < nextValue >).
features: [Symbol.iterator]
---*/

var iter = {};

var returnCount = 0;
var nextCount = 0;

iter[Symbol.iterator] = function() {
  return {
    next: function() {
      nextCount += 1;
      return {
        done: false
      };
    },
    return: function() {
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

Promise.all(iter);

assert.sameValue(nextCount, 0);
assert.sameValue(returnCount, 1);
