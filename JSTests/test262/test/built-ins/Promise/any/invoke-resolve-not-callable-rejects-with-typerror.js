// Copyright (C) 2019 Sergey Rubanov. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  If the constructor's `resolve` method is not callable, reject with a TypeError.
esid: sec-promise.any
info: |
  5. Let result be PerformPromiseAny(iteratorRecord, C, promiseCapability).

  Runtime Semantics: PerformPromiseAny

  6. Let promiseResolve be ? Get(constructor, "resolve").
  7. If ! IsCallable(promiseResolve) is false, throw a TypeError exception.

flags: [async]
features: [Promise.any, arrow-function]
---*/

Promise.resolve = null;

Promise.any([1])
  .then(
    () => $DONE('The promise should not be resolved.'),
    error => {
      assert(error instanceof TypeError);
    }
  ).then($DONE, $DONE);
