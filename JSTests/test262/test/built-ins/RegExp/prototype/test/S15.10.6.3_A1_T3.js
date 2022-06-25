// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Equivalent to the expression RegExp.prototype.exec(string) != null
es5id: 15.10.6.3_A1_T3
description: >
    RegExp is /a[a-z]{2,4}/ and tested string is new
    Object("abcdefghi")
---*/

var __string = new Object("abcdefghi");
var __re = /a[a-z]{2,4}/;

assert.sameValue(
  __re.test(__string),
  __re.exec(__string) !== null,
  '__re.test("new Object("abcdefghi")") must return __re.exec(__string) !== null'
);
