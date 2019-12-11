// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getMonth" is 0
esid: sec-date.prototype.getmonth
description: The "length" property of the "getMonth" is 0
---*/

if (Date.prototype.getMonth.hasOwnProperty("length") !== true) {
  $ERROR('#1: The getMonth has a "length" property');
}

if (Date.prototype.getMonth.length !== 0) {
  $ERROR('#2: The "length" property of the getMonth is 0');
}
