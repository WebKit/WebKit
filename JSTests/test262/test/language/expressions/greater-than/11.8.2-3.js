// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.8.2-3
description: >
    11.8.2 Greater-than Operator - Partial left to right order
    enforced when using Greater-than operator: toString > valueOf
---*/

var accessed = false;
var obj1 = {
  toString: function () {
    accessed = true;
    return 3;
  }
};
var obj2 = {
  valueOf: function () {
    return 4;
  }
};

assert.sameValue(obj1 > obj2, false, 'The result of (obj1 > obj2) is false');
assert.sameValue(accessed, true, 'The value of accessed is true');
