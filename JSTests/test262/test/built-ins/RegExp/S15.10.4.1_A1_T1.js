// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If pattern is an object R whose [[Class]] property is "RegExp" and flags is undefined, then let P be
    the pattern used to construct R and let F be the flags used to construct R
es5id: 15.10.4.1_A1_T1
description: Pattern is /./i and RegExp is new RegExp(pattern)
---*/

var __pattern = /./i;
var __re = new RegExp(__pattern); 

//CHECK#1
if (__re.source !== __pattern.source) {
  throw new Test262Error('#1: __pattern = /./i; __re = new RegExp(__pattern); __re.source === __pattern.source. Actual: '+ (__re.source));
}

//CHECK#2
if (__re.multiline !== __pattern.multiline) {
  throw new Test262Error('#2: __pattern = /./i; __re = new RegExp(__pattern); __re.multiline === __pattern.multiline. Actual: ' + (__re.multiline));
}

//CHECK#3
if (__re.global !== __pattern.global) {
  throw new Test262Error('#3: __pattern = /./i; __re = new RegExp(__pattern); __re.global === __pattern.global. Actual: ' + (__re.global));
}

//CHECK#4
if (__re.ignoreCase !== __pattern.ignoreCase) {
  throw new Test262Error('#4: __pattern = /./i; __re = new RegExp(__pattern); __re.ignoreCase === __pattern.ignoreCase. Actual: ' + (__re.ignoreCase));
}
