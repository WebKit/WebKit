// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-dataview.prototype.getuint32
description: >
  DataView.prototype.getUint32.name is "getUint32".
info: |
  DataView.prototype.getUint32 ( byteOffset [ , littleEndian ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(DataView.prototype.getUint32.name, "getUint32");

verifyNotEnumerable(DataView.prototype.getUint32, "name");
verifyNotWritable(DataView.prototype.getUint32, "name");
verifyConfigurable(DataView.prototype.getUint32, "name");
