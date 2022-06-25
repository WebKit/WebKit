// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-regexp.prototype.source
description: >
  get RegExp.prototype.source.name is "get source".
info: |
  get RegExp.prototype.source

  17 ECMAScript Standard Built-in Objects

  Functions that are specified as get or set accessor functions of built-in
  properties have "get " or "set " prepended to the property name string.

  Unless otherwise specified, the name property of a built-in function object,
  if it exists, has the attributes { [[Writable]]: false, [[Enumerable]]: false,
  [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

var get = Object.getOwnPropertyDescriptor(RegExp.prototype, 'source').get;

verifyProperty(get, 'name', {
  value: 'get source',
  writable: false,
  enumerable: false,
  configurable: true,
});
