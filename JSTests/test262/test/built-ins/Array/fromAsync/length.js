// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.fromasync
description: Value and property descriptor of Array.fromAsync.length
info: |
  Every built-in function object, including constructors, has a *"length"*
  property whose value is a non-negative integral Number. Unless otherwise
  specified, this value is equal to the number of required parameters shown in
  the subclause heading for the function description. Optional parameters and
  rest parameters are not included in the parameter count.

  Unless otherwise specified, the *"length"* property of a built-in function
  object has the attributes { [[Writable]]: *false*, [[Enumerable]]: *false*,
  [[Configurable]]: *true* }.
includes: [propertyHelper.js]
features: [Array.fromAsync]
---*/

verifyProperty(Array.fromAsync, "length", {
  value: 1,
  writable: false,
  enumerable: false,
  configurable: true,
});
