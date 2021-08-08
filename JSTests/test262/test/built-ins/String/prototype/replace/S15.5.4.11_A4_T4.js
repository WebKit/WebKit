// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: replace with regexp /([a-z]+)([0-9]+)/ and replace function returns
es5id: 15.5.4.11_A4_T4
description: searchValue is /([a-z]+)([0-9]+)/ig
---*/

var __str = "aBc12 dEf34";
var __pattern = /([a-z]+)([0-9]+)/ig;

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (__str.replace(__pattern, __replFN) !== '12aBc 34dEf') {
  throw new Test262Error('#1: var __str = "aBc12 dEf34"; var __pattern = /([a-z]+)([0-9]+)/ig; function __replFN() {return arguments[2] + arguments[1];}; __str.replace(__pattern, __replFN)===\'12aBc 34dEf\'. Actual: ' + __str.replace(__pattern, __replFN));
}
//
//////////////////////////////////////////////////////////////////////////////

function __replFN() {
  return arguments[2] + arguments[1];
}
