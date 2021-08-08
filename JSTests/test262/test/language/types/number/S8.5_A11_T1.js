// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The integer 0 has two representations, +0 and -0
es5id: 8.5_A11_T1
description: Check 1.0/p_zero !== 1.0/n_zero
---*/

var p_zero=+0;
var n_zero=-0;

if (1.0/p_zero === 1.0/n_zero){
  throw new Test262Error('#1: var p_zero=+0; var n_zero=-0; 1.0/p_zero !== 1.0/n_zero');
}
