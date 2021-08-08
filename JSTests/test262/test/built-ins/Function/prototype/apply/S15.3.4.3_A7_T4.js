// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If argArray is either an array or an arguments object,
    the function is passed the (ToUint32(argArray.length)) arguments argArray[0], argArray[1],...,argArray[ToUint32(argArray.length)-1]
es5id: 15.3.4.3_A7_T4
description: >
    argArray is (empty object, ( function(){return arguments;})
    ("a","b","c"))
---*/

var i = 0;

var p = {
  toString: function() {
    return "a" + (++i);
  }
};

var obj = {};

new Function(p, p, p, "this.shifted=a3;").apply(obj, (function() {
  return arguments;
})("a", "b", "c"));

//CHECK#1
if (obj["shifted"] !== "c") {
  throw new Test262Error('#1: If argArray is either an array or an arguments object, the function is passed the...');
}

//CHECK#2
if (typeof this["shifted"] !== "undefined") {
  throw new Test262Error('#2: If argArray is either an array or an arguments object, the function is passed the...');
}
