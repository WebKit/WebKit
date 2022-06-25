// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-set.prototype.forEach
description: >
    Set.prototype.forEach ( callbackfn [ , thisArg ] )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof Set.prototype.forEach,
  "function",
  "`typeof Set.prototype.forEach` is `'function'`"
);

verifyNotEnumerable(Set.prototype, "forEach");
verifyWritable(Set.prototype, "forEach");
verifyConfigurable(Set.prototype, "forEach");
