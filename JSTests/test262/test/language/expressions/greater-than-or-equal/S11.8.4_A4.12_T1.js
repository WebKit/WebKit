// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If neither x, nor y is a prefix of each other, returned result of strings
    comparison applies a simple lexicographic ordering to the sequences of
    code point value values
es5id: 11.8.4_A4.12_T1
description: x and y are string primitives
---*/

//CHECK#1
if (("xy" >= "xx") !== true) {
  throw new Test262Error('#1: ("xy" >= "xx") === true');
}

//CHECK#2
if (("xx" >= "xy") !== false) {
  throw new Test262Error('#2: ("xx" >= "xy") === false');
}

//CHECK#3
if (("y" >= "x") !== true) {
  throw new Test262Error('#3: ("y" >= "x") === true');
}

//CHECK#4
if (("aba" >= "aab") !== true) {
  throw new Test262Error('#4: ("aba" >= aab") === true');
}

//CHECK#5
if (("\u0061\u0061\u0061\u0061" >= "\u0061\u0061\u0061\u0062") !== false) {
  throw new Test262Error('#5: ("\\u0061\\u0061\\u0061\\u0061" >= \\u0061\\u0061\\u0061\\u0062") === false');
}

//CHECK#6
if (("a\u0000b" >= "a\u0000a") !== true) {
  throw new Test262Error('#6: ("a\\u0000b" >= "a\\u0000a") === true');
}

//CHECK#7
if (("aa" >= "aB") !== true) {
  throw new Test262Error('#7: ("aa" >= aB") === true');
}
