// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If thisArg is not null(defined) the called function is passed
    ToObject(thisArg) as the this value
es5id: 15.3.4.3_A5_T8
description: thisArg is Function()
---*/

var obj = Function();

new Function("this.touched= true; return this;").apply(obj);

assert(obj.touched, 'The value of obj.touched is expected to be true');
