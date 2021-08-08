// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The initial value of Error.prototype.constructor is the built-in Error
    constructor
es5id: 15.11.4.1_A1_T2
description: >
    Checking if creating "new Error.prototype.constructor" passes and
    checking its properties
---*/

var constr = Error.prototype.constructor;

var err = new constr;

//////////////////////////////////////////////////////////////////////////////
// CHECK#0
if (err === undefined) {
  throw new Test262Error('#0: constr = Error.prototype.constructor; err = new constr; err === undefined');
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// CHECK#1
if (err.constructor !== Error) {
  throw new Test262Error('#1: constr = Error.prototype.constructor; err = new constr; err.constructor === Error. Actual: ' + err.constructor);
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// CHECK#2
if (!(Error.prototype.isPrototypeOf(err))) {
  throw new Test262Error('#2: constr = Error.prototype.constructor; err = new constr; Error.prototype.isPrototypeOf(err) return true. Actual: ' + Error.prototype.isPrototypeOf(err));
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// CHECK#3
Error.prototype.toString = Object.prototype.toString;
var to_string_result = '[object ' + 'Error' + ']';
if (err.toString() !== to_string_result) {
  throw new Test262Error('#3: constr = Error.prototype.constructor; err = new constr; Error.prototype.toString=Object.prototype.toString; err.toString() === \'[object Error]\'. Actual: ' + err.toString());
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// CHECK#4
if (err.valueOf().toString() !== to_string_result) {
  throw new Test262Error('#4: constr = Error.prototype.constructor; err = new constr; Error.prototype.toString=Object.prototype.toString; err.valueOf().toString() === \'[object Error]\'. Actual: ' + err.valueOf().toString());
}
//
//////////////////////////////////////////////////////////////////////////////
