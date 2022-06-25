// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-typedarray.prototype
description: >
  The initial value of Float64Array.prototype is the Float64Array prototype object.
info: |
  The initial value of TypedArray.prototype is the corresponding TypedArray prototype intrinsic object (22.2.6).

  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
includes: [propertyHelper.js]
features: [TypedArray]
---*/

assert.sameValue(Float64Array.prototype, Object.getPrototypeOf(new Float64Array(0)));

verifyNotEnumerable(Float64Array, "prototype");
verifyNotWritable(Float64Array, "prototype");
verifyNotConfigurable(Float64Array, "prototype");
