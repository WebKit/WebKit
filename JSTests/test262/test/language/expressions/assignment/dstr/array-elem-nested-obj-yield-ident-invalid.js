// This file was procedurally generated from the following sources:
// - src/dstr-assignment/array-elem-nested-obj-yield-ident-invalid.case
// - src/dstr-assignment/syntax/assignment-expr.template
/*---
description: When a `yield` token appears within the Initializer of a nested destructuring assignment outside of a generator function body, it behaves as a IdentifierReference. (AssignmentExpression)
esid: sec-variable-statement-runtime-semantics-evaluation
features: [destructuring-binding]
flags: [generated, onlyStrict]
negative:
  phase: parse
  type: SyntaxError
info: |
    VariableDeclaration : BindingPattern Initializer

    1. Let rhs be the result of evaluating Initializer.
    2. Let rval be GetValue(rhs).
    3. ReturnIfAbrupt(rval).
    4. Return the result of performing BindingInitialization for
       BindingPattern passing rval and undefined as arguments.
---*/
$DONOTEVALUATE();

0, [{ x = yield }] = [{}];
