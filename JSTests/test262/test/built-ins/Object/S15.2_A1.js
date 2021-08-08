// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Object is the property of global
es5id: 15.2_A1
description: Checking if Object equals to this.Object
---*/

var obj = Object;

var thisobj = this.Object;

if (obj !== thisobj) {
  throw new Test262Error('Object is the property of global');
}
