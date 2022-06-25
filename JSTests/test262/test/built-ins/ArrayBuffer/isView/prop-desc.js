// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer.isview
description: >
  "isView" property of ArrayBuffer
info: |
  ES6 section 17: Every other data property described in clauses 18 through 26
  and in Annex B.2 has the attributes { [[Writable]]: true,
  [[Enumerable]]: false, [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
---*/

verifyNotEnumerable(ArrayBuffer, "isView");
verifyWritable(ArrayBuffer, "isView");
verifyConfigurable(ArrayBuffer, "isView");
