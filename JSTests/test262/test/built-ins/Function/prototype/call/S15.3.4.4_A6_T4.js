// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The call method takes one or more arguments, thisArg and (optionally) arg1, arg2 etc, and performs
    a function call using the [[Call]] property of the object
es5id: 15.3.4.4_A6_T4
description: >
    Argunemts of call function is (empty object, ( function(){return
    arguments;})("a","b","c","d"),"",2)
---*/

var i = 0;

var p = {
  toString: function() {
    return "a" + (++i);
  }
};

var obj = {};

new Function(p, p, p, "this.shifted=a3+a2+a1.length;").call(obj, (function() {
  return arguments;
})("a", "b", "c", "d"), "", 2);

//CHECK#1
if (obj["shifted"] !== "24") {
  throw new Test262Error('#1: The call method takes one or more arguments, thisArg and (optionally) arg1, arg2 etc, and performs a function call using the [[Call]] property of the object');
}

//CHECK#2
if (typeof this["shifted"] !== "undefined") {
  throw new Test262Error('#2: The call method takes one or more arguments, thisArg and (optionally) arg1, arg2 etc, and performs a function call using the [[Call]] property of the object');
}
