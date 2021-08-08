// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "DecimalLiteral :: .DecimalDigits ExponentPart"
es5id: 7.8.3_A2.2_T8
description: "ExponentPart :: E 0"
---*/

//CHECK#0
if (.0E0 !== 0.0) {
  throw new Test262Error('#0: .0E0 === 0.0');
}

//CHECK#1
if (.1E0 !== 0.1) {
  throw new Test262Error('#1: .1E0 === 0.1');
}

//CHECK#2
if (.2E0 !== 0.2) {
  throw new Test262Error('#2: .2E0 === 0.2');
}

//CHECK#3
if (.3E0 !== 0.3) {
  throw new Test262Error('#3: .3E0 === 0.3');
}

//CHECK#4
if (.4E0 !== 0.4) {
  throw new Test262Error('#4: .4E0 === 0.4');
}

//CHECK#5
if (.5E0 !== 0.5) {
  throw new Test262Error('#5: .5E0 === 0.5');
}

//CHECK#6
if (.6E0 !== 0.6) {
  throw new Test262Error('#6: .6E0 === 0.6');
}

//CHECK#7
if (.7E0 !== 0.7) {
  throw new Test262Error('#7: .7E0 === 0.7');
}

//CHECK#8
if (.8E0 !== 0.8) {
  throw new Test262Error('#8: .8E0 === 0.8');
}

//CHECK#9
if (.9E0 !== 0.9) {
  throw new Test262Error('#9: .9E0 === 0.9');
}
