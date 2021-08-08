// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCMilliseconds" is 1
esid: sec-date.prototype.setutcmilliseconds
description: The "length" property of the "setUTCMilliseconds" is 1
---*/

if (Date.prototype.setUTCMilliseconds.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The setUTCMilliseconds has a "length" property');
}

if (Date.prototype.setUTCMilliseconds.length !== 1) {
  throw new Test262Error('#2: The "length" property of the setUTCMilliseconds is 1');
}
