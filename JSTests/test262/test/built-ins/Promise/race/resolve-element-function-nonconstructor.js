// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-promise.race-resolve-element-functions
description: Promise.race Resolve Element functions are not constructors
info: |
  17 ECMAScript Standard Built-in Objects:
    Built-in function objects that are not identified as constructors do not
    implement the [[Construct]] internal method unless otherwise specified
    in the description of a particular function.
---*/

let resolveElementFunction;
let thenable = {
  then(fulfill) {
    resolveElementFunction = fulfill;
  }
};

function NotPromise(executor) {
  executor(() => {}, () => {});
}
NotPromise.resolve = function(v) {
  return v;
};
Promise.race.call(NotPromise, [thenable]);

assert.sameValue(Object.prototype.hasOwnProperty.call(resolveElementFunction, 'prototype'), false);
assert.throws(TypeError, () => {
  new resolveElementFunction();
});
