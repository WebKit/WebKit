// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype has the property "getUTCDate"
esid: sec-properties-of-the-date-prototype-object
es5id: 15.9.5_A15_T1
description: The Date.prototype has the property "getUTCDate"
---*/

if (Date.prototype.hasOwnProperty("getUTCDate") !== true) {
  $ERROR('#1: The Date.prototype has the property "getUTCDate"');
}
