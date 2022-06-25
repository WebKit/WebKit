// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If S contains any character that is not a radix-R digit,
    then let Z be the substring of S consisting of all characters before
    the first such character; otherwise, let Z be S
esid: sec-parseint-string-radix
description: Complex test. Radix-R notation in [0..9, A-Z]
---*/

//CHECK#
var R_digit = ["1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"];
for (var i = 2; i <= 36; i++) {
  assert.sameValue(parseInt(R_digit[i - 2] + "$", i), i - 1, 'parseInt(R_digit[i - 2] + "$", 2) must return i - 1');
}
