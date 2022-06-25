// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "RegularExpressionFlags :: IdentifierPart"
es5id: 7.8.5_A3.1_T4
description: "IdentifierPart :: gi"
---*/

//CHECK#1
var regexp = /(?:)/gi; 
if (regexp.global !== true) {
  throw new Test262Error('#1: var regexp = /(?:)/g; regexp.global === true. Actual: ' + (regexp.global));
}

//CHECK#2 
if (regexp.ignoreCase !== true) {
  throw new Test262Error('#2: var regexp = /(?:)/g; regexp.ignoreCase === true. Actual: ' + (regexp.ignoreCase));
}

//CHECK#3
if (regexp.multiline !== false) {
  throw new Test262Error('#3: var regexp = /(?:)/g; regexp.multiline === false. Actual: ' + (regexp.multiline));
}
