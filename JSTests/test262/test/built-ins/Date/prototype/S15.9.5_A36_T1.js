// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype has the property "setDate"
esid: sec-properties-of-the-date-prototype-object
description: The Date.prototype has the property "setDate"
---*/

if (Date.prototype.hasOwnProperty("setDate") !== true) {
  $ERROR('#1: The Date.prototype has the property "setDate"');
}
