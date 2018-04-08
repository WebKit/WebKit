// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype has the property "toDateString"
esid: sec-properties-of-the-date-prototype-object
es5id: 15.9.5_A03_T1
description: The Date.prototype has the property "toDateString"
---*/

if (Date.prototype.hasOwnProperty("toDateString") !== true) {
  $ERROR('#1: The Date.prototype has the property "toDateString"');
}
