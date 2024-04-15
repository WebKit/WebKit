// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-613
description: ES5 Attributes - all attributes in Object.lastIndexOf are correct
includes: [propertyHelper.js]
---*/

verifyProperty(Array.prototype, "lastIndexOf", {
  writable: true,
  enumerable: false,
  configurable: true,
});
