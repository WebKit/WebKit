// Copyright 2016 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-applying-the-exp-operator
description: If base is −∞ and exponent < 0 and exponent is an odd integer, the result is −0.
---*/


var base = -Infinity;
var exponents = [];
exponents[2] = -1;
exponents[1] = -111;
exponents[0] = -111111;

for (var i = 0; i < exponents.length; i++) {
  if ((base ** exponents[i]) !== -0) {
    throw new Test262Error("(" + base + " ** " + exponents[i] + ") !== -0");
  }
}
