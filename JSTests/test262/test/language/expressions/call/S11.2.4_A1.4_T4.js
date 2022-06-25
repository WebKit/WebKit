// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "Arguments : (ArgumentList : ArgumentList, AssignmentExpression)"
es5id: 11.2.4_A1.4_T4
description: >
    Return an internal list whose length is one greater than the
    length of ArgumentList and whose items are the items of
    ArgumentList, in order,  followed at the end by
    GetValue(AssignmentExpression), which is the last item of  the new
    list
---*/

var x = function () { throw "x"; };
var y = function () { throw "y"; };

function f_arg() {
}

//CHECK#1
try {
  f_arg(x(),y());
  throw new Test262Error('#1.1: var x = { valueOf: function () { throw "x"; } }; var y = { valueOf: function () { throw "y"; } }; function f_arg() {} f_arg(x(),y()) throw "x". Actual: ' + (f_arg(x(),y())));  
}
catch (e) {
  if (e === "y") {
     throw new Test262Error('#1.2: First argument is evaluated first, and then second argument');
   } else {
     if (e !== "x") {
       throw new Test262Error('#1.3: var x = { valueOf: function () { throw "x"; } }; var y = { valueOf: function () { throw "y"; } }; function f_arg() {} f_arg(x(),y()) throw "x". Actual: ' + (e));
     }
   }
}
