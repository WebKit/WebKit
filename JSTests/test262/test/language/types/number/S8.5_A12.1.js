// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: +Infinity and Infinity are the same as Number.POSITIVE_INFINITY
es5id: 8.5_A12.1
description: Compare Infinity and +Infinity with Number.POSITIVE_INFINITY
---*/

var p_inf=+Infinity;
var inf=Infinity;

//CHECK #1
if (p_inf!==Number.POSITIVE_INFINITY){
  throw new Test262Error('#1: +Infinity is the same as Number.POSITIVE_INFINITY');
}

//CHECK #2
if (inf!==Number.POSITIVE_INFINITY){
  throw new Test262Error('#2: Infinity is the same as Number.POSITIVE_INFINITY');
}
