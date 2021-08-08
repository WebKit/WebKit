// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The initial value of Boolean.prototype.constructor is the
    built-in Boolean constructor
esid: sec-boolean-constructor
description: Compare Boolean.prototype.constructor with Boolean
---*/

//CHECK#1
if (Boolean.prototype.constructor !== Boolean) {
  throw new Test262Error('#1: Boolean.prototype.constructor === Boolean');
}
