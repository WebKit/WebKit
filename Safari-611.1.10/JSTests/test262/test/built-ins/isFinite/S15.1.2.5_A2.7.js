// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The isFinite property can't be used as constructor
esid: sec-isfinite-number
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

//CHECK#1

try {
  new isFinite();
  $ERROR('#1.1: new isFinite() throw TypeError. Actual: ' + (new isFinite()));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    $ERROR('#1.2: new isFinite() throw TypeError. Actual: ' + (e));
  }
}
