// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.group
description: Array.prototype.group property name descriptor
info: |
  22.1.3.14 Array.prototype.group ( callbackfn [ , thisArg ] )

  ...

    17 ECMAScript Standard Built-in Objects

  ...

includes: [propertyHelper.js]
features: [array-grouping]
---*/

verifyProperty(Array.prototype.group, "name", {
  value: "group",
  enumerable: false,
  writable: false,
  configurable: true
});
