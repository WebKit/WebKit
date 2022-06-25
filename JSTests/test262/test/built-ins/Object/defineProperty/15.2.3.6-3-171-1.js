// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-3-171-1
description: >
    Object.defineProperty - 'Attributes' is a Date object that uses
    Object's [[Get]] method to access the 'writable' property of
    prototype object (8.10.5 step 6.b)
includes: [propertyHelper.js]
---*/

var obj = {};
try {
  Date.prototype.writable = true;

  var dateObj = new Date();

  Object.defineProperty(obj, "property", dateObj);
  verifyWritable(obj, "property");

} finally {
  delete Date.prototype.writable;
}
