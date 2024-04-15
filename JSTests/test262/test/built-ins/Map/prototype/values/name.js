// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-map.prototype.values
description: >
  Map.prototype.values.name value and descriptor.
info: |
  Map.prototype.values ()

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

verifyProperty(Map.prototype.values, "name", {
  value: "values",
  writable: false,
  enumerable: false,
  configurable: true
});
