// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.3.4.5.1-4-9
description: >
    [[Call]] - length of parameters of 'target' is 0, length of
    'boundArgs' is 1, length of 'ExtraArgs' is 1, and with 'boundThis'
---*/

var obj = {
  prop: "abc"
};

var func = function() {
  return this === obj && arguments[0] === 1 && arguments[1] === 2;
};

var newFunc = Function.prototype.bind.call(func, obj, 1);

assert(newFunc(2), 'newFunc(2) !== true');
