// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-dataview.prototype.getfloat64
description: >
  DataView.prototype.getFloat64.name is "getFloat64".
info: |
  DataView.prototype.getFloat64 ( byteOffset [ , littleEndian ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(DataView.prototype.getFloat64.name, "getFloat64");

verifyNotEnumerable(DataView.prototype.getFloat64, "name");
verifyNotWritable(DataView.prototype.getFloat64, "name");
verifyConfigurable(DataView.prototype.getFloat64, "name");
