// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: |
    Promise prototype.catch is a function
es6id: S25.4.5.1_A1.1_T1
author: Sam Mikes
description: Promise.prototype.catch is a function
---*/

if (!(Promise.prototype.catch instanceof Function)) {
  throw new Test262Error("Expected Promise.prototype.catch to be a function");
}
