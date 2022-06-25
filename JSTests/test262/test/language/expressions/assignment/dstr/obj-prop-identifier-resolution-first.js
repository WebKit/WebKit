// This file was procedurally generated from the following sources:
// - src/dstr-assignment/obj-prop-identifier-resolution-first.case
// - src/dstr-assignment/default/assignment-expr.template
/*---
description: Evaluation of DestructuringAssignmentTarget (first of many). (AssignmentExpression)
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
var x = null;
var y;

var result;
var vals = { a: 3 };

result = { a: x, y } = vals;

assert.sameValue(x, 3);

assert.sameValue(result, vals);
