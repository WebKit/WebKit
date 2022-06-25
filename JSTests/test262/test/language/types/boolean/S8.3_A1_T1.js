// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Boolean type have two values, called true and false
es5id: 8.3_A1_T1
description: Assign true and false to variables
---*/

if (x !== undefined) {
    throw new Test262Error("#0 x !== undefined, but actual is "+ x);
}

////////////////////////////////////////////////////////////////////////
// CHECK#1
var x = true;
var y = false;

if (x !== true) {
    throw new Test262Error("#1.1 x !== true, but actual is "+ x);
}

if (y !== false) {
    throw new Test262Error("#1.1 y !== false, but actual is "+ y);
}

//
////////////////////////////////////////////////////////////////////////
