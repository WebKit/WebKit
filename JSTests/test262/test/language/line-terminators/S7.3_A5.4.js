// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Multi line comment can contain LINE SEPARATOR (U+2029)
es5id: 7.3_A5.4
description: Insert PARAGRAPH SEPARATOR (U+2029) into multi line comment
---*/

// CHECK#1
eval("/*\u2029 multi line \u2029 comment \u2029*/");

//CHECK#2
var x = 0;
eval("/*\u2029 multi line \u2029 comment \u2029 x = 1;*/");
if (x !== 0) {
  throw new Test262Error('#1: var x = 0; eval("/*\\u2029 multi line \\u2029 comment \\u2029 x = 1;*/"); x === 0. Actual: ' + (x));
}
