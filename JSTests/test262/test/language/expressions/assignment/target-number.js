// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-assignment-operators-static-semantics-early-errors
es5id: 11.13.1-1-1
description: >
    simple assignment throws ReferenceError if LeftHandSide is not a
    reference (number)
info: |
    AssignmentExpression : LeftHandSideExpression = AssignmentExpression

    It is an early Reference Error if LeftHandSideExpression is neither an
    ObjectLiteral nor an ArrayLiteral and IsValidSimpleAssignmentTarget of
    LeftHandSideExpression is false.
negative:
  phase: parse
  type: ReferenceError
---*/

throw "Test262: This statement should not be evaluated.";

42 = 42;
