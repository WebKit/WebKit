// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If S contains any character that is not a radix-R digit,
    then let Z be the substring of S consisting of all characters before
    the first such character; otherwise, let Z be S
esid: sec-parseint-string-radix
description: Complex test. Radix-R notation in [0..9, a-z]
---*/

//CHECK#
var R_digit = ["1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"];
for (var i = 2; i <= 35; i++) {
  assert.sameValue(
    parseInt(R_digit[i - 2] + R_digit[i - 1], i),
    i - 1,
    'parseInt(R_digit[i - 2] + R_digit[i - 1], i) must return i - 1'
  );
}
