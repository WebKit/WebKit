// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Number prototype object has the property constructor
es5id: 15.7.4_A3.1
description: The test uses hasOwnProperty() method
---*/

//CHECK#1
if (Number.prototype.hasOwnProperty("constructor") !== true) {
  throw new Test262Error('#1: The Number prototype object has the property constructor');
}
