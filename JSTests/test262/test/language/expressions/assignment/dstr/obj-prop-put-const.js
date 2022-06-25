// This file was procedurally generated from the following sources:
// - src/dstr-assignment/obj-prop-put-const.case
// - src/dstr-assignment/error/assignment-expr.template
/*---
description: The assignment target should obey `const` semantics. (AssignmentExpression)
esid: sec-variable-statement-runtime-semantics-evaluation
features: [const, destructuring-binding]
flags: [generated]
info: |
    VariableDeclaration : BindingPattern Initializer

    1. Let rhs be the result of evaluating Initializer.
    2. Let rval be GetValue(rhs).
    3. ReturnIfAbrupt(rval).
    4. Return the result of performing BindingInitialization for
       BindingPattern passing rval and undefined as arguments.
---*/
const c = 1;

assert.throws(TypeError, function() {
  0, { a: c } = { a: 2 };
});
