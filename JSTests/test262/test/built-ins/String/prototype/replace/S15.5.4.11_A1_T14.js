// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: String.prototype.replace (searchValue, replaceValue)
es5id: 15.5.4.11_A1_T14
description: Instance is string, searchValue is regular expression
---*/

var __reg = new RegExp("77");

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if ("ABB\u0041BABAB\u0037\u0037BBAA".replace(__reg, 1) !== "ABBABABAB\u0031BBAA") {
  throw new Test262Error('#1: var __reg = new RegExp("77"); "ABB\\u0041BABAB\\u0037\\u0037BBAA".replace(__reg, 1) === "ABBABABAB\\u0031BBAA". Actual: ' + ("ABB\u0041BABAB\u0037\u0037BBAA".replace(__reg, 1)));
}
//
//////////////////////////////////////////////////////////////////////////////
