// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-dataview.prototype.setuint8
description: >
  Return abrupt from ToNumber(symbol byteOffset)
info: |
  24.2.4.18 DataView.prototype.setUint8 ( byteOffset, value )

  1. Let v be the this value.
  2. Return ? SetViewValue(v, byteOffset, true, "Uint8", value).

  24.2.1.2 SetViewValue ( view, requestIndex, isLittleEndian, type, value )

  ...
  4. Let numberIndex be ? ToNumber(requestIndex).
  ...
features: [Symbol]
---*/

var buffer = new ArrayBuffer(1);
var sample = new DataView(buffer, 0);

var s = Symbol("1");

assert.throws(TypeError, function() {
  sample.setUint8(s, 1);
});
