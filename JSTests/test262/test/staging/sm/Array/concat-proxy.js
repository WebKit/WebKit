// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1287520;
var summary = 'Array.prototype.concat should check HasProperty everytime for non-dense array';

print(BUGNUMBER + ": " + summary);

var a = [1, 2, 3];
a.constructor = {
  [Symbol.species]: function(...args) {
    var p = new Proxy(new Array(...args), {
      defineProperty(target, propertyKey, receiver) {
        if (propertyKey === "0") delete a[1];
        return Reflect.defineProperty(target, propertyKey, receiver);
      }
    });
    return p;
  }
};

var p = a.concat();
assert.sameValue(0 in p, true);
assert.sameValue(1 in p, false);
assert.sameValue(2 in p, true);

