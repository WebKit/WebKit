// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.14
description: >
  Reflect.setPrototypeOf.length value and property descriptor
includes: [propertyHelper.js]
features: [Reflect, Reflect.setPrototypeOf]
---*/

verifyProperty(Reflect.setPrototypeOf, "length", {
  value: 2,
  writable: false,
  enumerable: false,
  configurable: true
});
