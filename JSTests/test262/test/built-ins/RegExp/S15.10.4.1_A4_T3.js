// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: let F be the empty string if flags is undefined
es5id: 15.10.4.1_A4_T3
description: Use undefined properties of object as flags of RegExp
---*/

var __re = new RegExp({}.p, {}.q);

//CHECK#2
if (__re.multiline !== false) {
  throw new Test262Error('#2: __re = new RegExp({}.p, {}.q); __re.multiline === false. Actual: ' + (__re.multiline));
}

//CHECK#3
if (__re.global !== false) {
  throw new Test262Error('#3: __re = new RegExp({}.p, {}.q); __re.global === false. Actual: ' + (__re.global));
}

//CHECK#4
if (__re.ignoreCase !== false) {
  throw new Test262Error('#4: __re = new RegExp({}.p, {}.q); __re.ignoreCase === false. Actual: ' + (__re.ignoreCase));
}
