// Copyright (C) 2024 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-uint8array.prototype.setfrombase64
description: Uint8Array.prototype.setFromBase64 throws a SyntaxError when input has non-base64, non-ascii-whitespace characters
features: [uint8array-base64, TypedArray]
---*/

var illegal = [
  'Zm.9v',
  'Zm9v^',
  'Zg==&',
  'Z−==', // U+2212 'Minus Sign'
  'Z＋==', // U+FF0B 'Fullwidth Plus Sign'
  'Zg\u00A0==', // nbsp
  'Zg\u2009==', // thin space
  'Zg\u2028==', // line separator
];
illegal.forEach(function(value) {
  assert.throws(SyntaxError, function() {
    var target = new Uint8Array([255, 255, 255, 255, 255]);
    target.setFromBase64(value);
  });
});
