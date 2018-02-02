// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: GetValue(V) mast fail
es5id: 8.7.2_A1_T1
description: Checking if execution of "'litera'=1;" fails
negative:
  phase: parse
  type: ReferenceError
---*/

throw "Test262: This statement should not be evaluated.";

'litera'=1;
