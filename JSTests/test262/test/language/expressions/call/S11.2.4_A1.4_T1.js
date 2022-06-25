// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "Arguments : (ArgumentList : ArgumentList, AssignmentExpression)"
es5id: 11.2.4_A1.4_T1
description: >
    Return an internal list whose length is one greater than the
    length of ArgumentList and whose items are the items of
    ArgumentList, in order,  followed at the end by
    GetValue(AssignmentExpression), which is the last item of  the new
    list
flags: [noStrict]
---*/

function f_arg() {
}

//CHECK#1
f_arg(x=1,x);
