// This file was procedurally generated from the following sources:
// - src/dstr-assignment/array-elision-val-array.case
// - src/dstr-assignment/default/assignment-expr.template
/*---
description: An ArrayAssignmentPattern containing only Elisions requires iterable values (AssignmentExpression)
esid: sec-variable-statement-runtime-semantics-evaluation
features: [destructuring-binding]
flags: [generated]
info: |
    VariableDeclaration : BindingPattern Initializer

    1. Let rhs be the result of evaluating Initializer.
    2. Let rval be GetValue(rhs).
    3. ReturnIfAbrupt(rval).
    4. Return the result of performing BindingInitialization for
       BindingPattern passing rval and undefined as arguments.
---*/

var result;
var vals = [];

result = [,] = vals;



assert.sameValue(result, vals);
