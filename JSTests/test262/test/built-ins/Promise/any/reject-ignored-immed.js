// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-promise.any
description: >
    Resolved promises ignore rejections through immediate invocation of the
    provided resolving function
info: |
  Let result be PerformPromiseAny(iteratorRecord, C, promiseCapability).

  Runtime Semantics: PerformPromiseAny

  ...
  Let remainingElementsCount be a new Record { [[value]]: 1 }.
  ...
  8.d ...
    ii. Set remainingElementsCount.[[value]] to remainingElementsCount.[[value]] − 1.
    iii. If remainingElementsCount.[[value]] is 0,
      1. Let error be a newly created AggregateError object.
      2. Perform ! DefinePropertyOrThrow(error, "errors", Property Descriptor { [[Configurable]]: true, [[Enumerable]]: false, [[Writable]]: true, [[Value]]: errors }).
      3. Return ThrowCompletion(error).
  ...

  Promise.any Reject Element Functions
  ...
  Let alreadyCalled be the value of F's [[AlreadyCalled]] internal slot.
  If alreadyCalled.[[value]] is true, return undefined.
  Set alreadyCalled.[[value]] to true.
  ...

flags: [async]
features: [Promise.any, arrow-function]
---*/

let fulfiller = {
  then(resolve) {
    resolve();
  }
};
let lateRejector = {
  then(resolve, reject) {
    resolve();
    reject();
  }
};

Promise.any([fulfiller, lateRejector])
  .then(() => {
    $DONE();
  }, () => {
    $DONE('The promise should not be rejected.');
  });
