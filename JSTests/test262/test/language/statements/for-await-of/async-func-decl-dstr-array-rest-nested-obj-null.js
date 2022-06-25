// This file was procedurally generated from the following sources:
// - src/dstr-assignment-for-await/array-rest-nested-obj-null.case
// - src/dstr-assignment-for-await/default/async-func-decl.template
/*---
description: When DestructuringAssignmentTarget is an object literal and the iterable emits `null` as the only value, an array with a single `null` element should be used as the value of the nested DestructuringAssignment. (for-await-of statement in an async function declaration)
esid: sec-for-in-and-for-of-statements-runtime-semantics-labelledevaluation
features: [destructuring-binding, async-iteration]
flags: [generated, async]
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
let x, length;

let iterCount = 0;
async function fn() {
  for await ([...{ 0: x, length }] of [[null]]) {
    assert.sameValue(x, null);
    assert.sameValue(length, 1);

    iterCount += 1;
  }
}

let promise = fn();

promise
  .then(() => assert.sameValue(iterCount, 1, 'iteration occurred as expected'), $DONE)
  .then($DONE, $DONE);
