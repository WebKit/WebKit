// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "The MV of StringNumericLiteral ::: [empty] is 0"
es5id: 9.3.1_A1
description: Number('') convert to Number by explicit transformation
---*/

// CHECK#1
if (Number("") !== 0) {
  throw new Test262Error('#1.1: Number("") === 0. Actual: ' + (Number("")));
} else {
  if (1 / Number("") !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#1.2: Number("") == +0. Actual: -0');
  }
}
