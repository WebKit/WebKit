// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "setUTCMilliseconds" has { DontEnum }
    attributes
esid: sec-date.prototype.setutcmilliseconds
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.setUTCMilliseconds === false) {
  $ERROR('#1: The Date.prototype.setUTCMilliseconds property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('setUTCMilliseconds')) {
  $ERROR('#2: The Date.prototype.setUTCMilliseconds property has not the attributes DontDelete');
}
