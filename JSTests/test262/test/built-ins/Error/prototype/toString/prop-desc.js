// Copyright (c) 2021 the V8 project authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-error.prototype.toString
description: Property descriptor of Error.prototype.toString
includes: [propertyHelper.js]
---*/

verifyProperty(Error.prototype, 'toString', {
  enumerable: false,
  writable: true,
  configurable: true,
});
