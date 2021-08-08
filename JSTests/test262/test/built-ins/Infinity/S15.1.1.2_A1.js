// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The initial value of Infinity is Number.POSITIVE_INFINITY
es5id: 15.1.1.2_A1
description: Use typeof, isNaN, isFinite
---*/

// CHECK#1
if (typeof(Infinity) !== "number") {
  throw new Test262Error('#1: typeof(Infinity) === "number". Actual: ' + (typeof(Infinity)));
}

// CHECK#2
if (isFinite(Infinity) !== false) {
  throw new Test262Error('#2: Infinity === Not-a-Finite. Actual: ' + (Infinity));
}

// CHECK#3
if (isNaN(Infinity) !== false) {
  throw new Test262Error('#3: Infinity === Not-a-Number. Actual: ' + (Infinity));
}


// CHECK#4
if (Infinity !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#4: Infinity === Number.POSITIVE_INFINITY. Actual: ' + (Infinity));
}
