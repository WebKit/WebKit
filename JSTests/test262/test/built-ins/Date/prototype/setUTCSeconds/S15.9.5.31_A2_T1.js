// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCSeconds" is 2
esid: sec-date.prototype.setutcseconds
description: The "length" property of the "setUTCSeconds" is 2
---*/

if (Date.prototype.setUTCSeconds.hasOwnProperty("length") !== true) {
  $ERROR('#1: The setUTCSeconds has a "length" property');
}

if (Date.prototype.setUTCSeconds.length !== 2) {
  $ERROR('#2: The "length" property of the setUTCSeconds is 2');
}
