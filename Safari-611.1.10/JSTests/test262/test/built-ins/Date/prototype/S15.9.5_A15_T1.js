// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype has the property "getUTCDate"
esid: sec-properties-of-the-date-prototype-object
description: The Date.prototype has the property "getUTCDate"
---*/

if (Date.prototype.hasOwnProperty("getUTCDate") !== true) {
  $ERROR('#1: The Date.prototype has the property "getUTCDate"');
}
