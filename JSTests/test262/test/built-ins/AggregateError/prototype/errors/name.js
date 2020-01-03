// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-aggregate-error.prototype.errors
description: >
  Property descriptor of (get AggregateError.prototype.errors).name
info: |
  get AggregateError.prototype.errors

  17 ECMAScript Standard Built-in Objects

  Functions that are specified as get or set accessor functions of built-in
  properties have "get " or "set " prepended to the property name string.
includes: [propertyHelper.js]
features: [AggregateError]
---*/

var desc = Object.getOwnPropertyDescriptor(
  AggregateError.prototype, 'errors'
);

verifyProperty(desc.get, 'name', {
  value: 'get errors',
  enumerable: false,
  writable: false,
  configurable: true
});
