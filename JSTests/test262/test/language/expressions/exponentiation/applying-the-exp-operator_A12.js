// Copyright 2016 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-applying-the-exp-operator
description: If base is +∞ and exponent < 0, the result is +0.
---*/


var base = +Infinity;
var exponents = [];
exponents[0] = -Infinity;
exponents[1] = -1.7976931348623157E308; //largest (by module) finite number
exponents[2] = -1;
exponents[3] = -0.000000000000001;

for (var i = 0; i < exponents.length; i++) {
  if ((base ** exponents[i]) !== +0) {
    throw new Test262Error("(" + base + " **  " + exponents[i] + ") !== +0");
  }
}
