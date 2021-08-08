// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Function is the property of global
es5id: 15.3_A1
description: Compare Function with this.Function
---*/

var obj = Function;

var thisobj = this.Function;

if (obj !== thisobj) {
  throw new Test262Error('Function is the property of global');
}
