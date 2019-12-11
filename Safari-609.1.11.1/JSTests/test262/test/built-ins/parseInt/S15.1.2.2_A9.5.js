// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The parseInt property has the attribute DontEnum
esid: sec-parseint-string-radix
description: Checking use propertyIsEnumerable, for-in
---*/

//CHECK#1
if (this.propertyIsEnumerable('parseInt') !== false) {
  $ERROR('#1: this.propertyIsEnumerable(\'parseInt\') === false. Actual: ' + (this.propertyIsEnumerable('parseInt')));
}

//CHECK#2
var result = true;
for (var p in this) {
  if (p === "parseInt") {
    result = false;
  }
}

if (result !== true) {
  $ERROR('#2: result = true; for (p in this) { if (p === "parseInt") result = false; }  result === true;');
}
