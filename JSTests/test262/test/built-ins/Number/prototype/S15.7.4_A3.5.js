// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Number prototype object has the property toFixed
es5id: 15.7.4_A3.5
description: The test uses hasOwnProperty() method
---*/

//CHECK#1
if (Number.prototype.hasOwnProperty("toFixed") !== true) {
  throw new Test262Error('#1: The Number prototype object has the property toFixed');
}
