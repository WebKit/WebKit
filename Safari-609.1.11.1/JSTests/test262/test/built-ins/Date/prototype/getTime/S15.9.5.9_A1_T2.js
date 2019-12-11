// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getTime" has { DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.getTime === false) {
  $ERROR('#1: The Date.prototype.getTime property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('getTime')) {
  $ERROR('#2: The Date.prototype.getTime property has not the attributes DontDelete');
}
