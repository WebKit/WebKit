// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Function.prototype.call.length property does not have the attribute
    DontDelete
es5id: 15.3.4.4_A9
description: >
    Checking if deleting the Function.prototype.call.length property
    fails
---*/

//CHECK#0
if (!(Function.prototype.call.hasOwnProperty('length'))) {
  throw new Test262Error('#0: the Function.prototype.call has length property');
}

//CHECK#1
if (!delete Function.prototype.call.length) {
  throw new Test262Error('#1: The Function.prototype.call.length property does not have the attributes DontDelete');
}

//CHECK#2
if (Function.prototype.call.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Function.prototype.call.length property does not have the attributes DontDelete');
}
