// This file was procedurally generated from the following sources:
// - src/dstr-assignment-for-await/array-rest-nested-array-yield-ident-valid.case
// - src/dstr-assignment-for-await/async-function/async-func-decl.template
/*---
description: When a `yield` token appears within the DestructuringAssignmentTarget of a nested destructuring assignment and outside of a generator function body, it should behave as an IdentifierExpression. (for-await-of statement in an async function declaration)
esid: sec-for-in-and-for-of-statements-runtime-semantics-labelledevaluation
features: [destructuring-binding, async-iteration]
flags: [generated, noStrict, async]
info: |
    IterationStatement :
      for await ( LeftHandSideExpression of AssignmentExpression ) Statement

    1. Let keyResult be the result of performing ? ForIn/OfHeadEvaluation(« »,
       AssignmentExpression, iterate).
    2. Return ? ForIn/OfBodyEvaluation(LeftHandSideExpression, Statement,
       keyResult, assignment, labelSet).

    13.7.5.13 Runtime Semantics: ForIn/OfBodyEvaluation

    [...]
    5. If destructuring is true and if lhsKind is assignment, then
       a. Assert: lhs is a LeftHandSideExpression.
       b. Let assignmentPattern be the parse of the source text corresponding to
          lhs using AssignmentPattern as the goal symbol.
    [...]
---*/
let yield = 'prop';
let x = {};

let iterCount = 0;
async function fn() {
  for await ([...[x[yield]]] of [[86]]) {
    assert.sameValue(x.prop, 86);

    iterCount += 1;
  }
}

let promise = fn();

promise
  .then(() => assert.sameValue(iterCount, 1, 'iteration occurred as expected'), $DONE)
  .then($DONE, $DONE);
