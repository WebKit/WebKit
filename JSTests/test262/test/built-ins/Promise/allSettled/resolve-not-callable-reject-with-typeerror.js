// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-promise.allSettled
description: >
  If the constructor's `resolve` method is not callable, reject with a TypeError.
info: |
  Let result be PerformPromiseAllSettled(iteratorRecord, C, promiseCapability).

  Runtime Semantics: PerformPromiseAllSettled

  Let promiseResolve be ? Get(constructor, "resolve").
  If ! IsCallable(promiseResolve) is false, throw a TypeError exception.

flags: [async]
features: [Promise.allSettled, arrow-function]
---*/

Promise.resolve = null;

Promise.allSettled([1])
  .then(
    () => $DONE('The promise should not be resolved.'),
    error => {
      assert(error instanceof TypeError);
    }
  ).then($DONE, $DONE);
