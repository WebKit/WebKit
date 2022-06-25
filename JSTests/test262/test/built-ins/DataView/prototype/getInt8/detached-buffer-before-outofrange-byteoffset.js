// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-dataview.prototype.getint8
description: >
  Detached buffer is checked before out of range byteOffset's value
info: |
  24.2.4.7 DataView.prototype.getInt8 ( byteOffset )

  1. Let v be the this value.
  2. Return ? GetViewValue(v, byteOffset, true, "Int8").

  24.2.1.1 GetViewValue ( view, requestIndex, isLittleEndian, type )

  ...
  6. Let buffer be view.[[ViewedArrayBuffer]].
  7. If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
  ...
  11. If getIndex + elementSize > viewSize, throw a RangeError exception.
  ...
includes: [detachArrayBuffer.js]
---*/

var sample;
var buffer = new ArrayBuffer(12);

sample = new DataView(buffer, 0);

$DETACHBUFFER(buffer);

assert.throws(TypeError, function() {
  sample.getInt8(13);
}, "13");
