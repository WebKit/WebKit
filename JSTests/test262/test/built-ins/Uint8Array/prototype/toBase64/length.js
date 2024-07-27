// Copyright (C) 2024 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-uint8array.prototype.tobase64
description: >
  Uint8Array.prototype.toBase64.length is 0.
includes: [propertyHelper.js]
features: [uint8array-base64, TypedArray]
---*/

verifyProperty(Uint8Array.prototype.toBase64, 'length', {
  value: 0,
  enumerable: false,
  writable: false,
  configurable: true
});
