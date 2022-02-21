// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The value of the length property is usually an integer that indicates the
    'typical' number of arguments expected by the function
es5id: 15.3.5.1_A1_T1
description: Checking length property of Function("arg1,arg2,arg3", null)
---*/

var f = new Function("arg1,arg2,arg3", null);

assert(f.hasOwnProperty('length'), 'f.hasOwnProperty(\'length\') must return true');
assert.sameValue(f.length, 3, 'The value of f.length is expected to be 3');
