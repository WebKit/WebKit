// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: String.prototype.search (regexp)
es5id: 15.5.4.12_A1_T5
description: Argument is null, and instance is function call, that return string
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
//since ToString(null) evaluates to "null" search(null) evaluates to search("null")
if (function() {
    return "gnulluna"
  }().search(null) !== 1) {
  throw new Test262Error('#1: function(){return "gnulluna"}().search(null) === 1. Actual: ' + function() {
    return "gnulluna"
  }().search(null));
}
//
//////////////////////////////////////////////////////////////////////////////
