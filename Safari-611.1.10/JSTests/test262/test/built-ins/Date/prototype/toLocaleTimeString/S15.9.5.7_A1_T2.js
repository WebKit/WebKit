// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "toLocaleTimeString" has { DontEnum }
    attributes
esid: sec-date.prototype.tolocaletimestring
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.toLocaleTimeString === false) {
  $ERROR('#1: The Date.prototype.toLocaleTimeString property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('toLocaleTimeString')) {
  $ERROR('#2: The Date.prototype.toLocaleTimeString property has not the attributes DontDelete');
}
