// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    For the keyword null the typeof operator returns the "object"
    See also
    http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Operators:Special_Operators:typeof_Operator
    and
    http://bugs.ecmascript.org/ticket/250
    for example
es5id: 8.2_A3
description: Check type of null
---*/

//////////////////////////////////////////////////////////////
// CHECK#1
if (typeof(null) !== "object") {
  throw new Test262Error('#1: typeof null === "object". Actual: ' + (typeof null));
}
//
/////////////////////////////////////////////////////////////
