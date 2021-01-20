// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-promise.race-resolve-element-functions
description: The [[Prototype]] of Promise.race Resolve Element functions
info: |
  17 ECMAScript Standard Built-in Objects:
    Unless otherwise specified every built-in function and every built-in
    constructor has the Function prototype object, which is the initial
    value of the expression Function.prototype (19.2.3), as the value of
    its [[Prototype]] internal slot.
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

assert.sameValue(Object.getPrototypeOf(resolveElementFunction), Function.prototype);
