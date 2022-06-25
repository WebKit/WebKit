// This file was procedurally generated from the following sources:
// - src/dstr-assignment/array-empty-iter-close-null.case
// - src/dstr-assignment/error/for-of.template
/*---
description: IteratorClose throws a TypeError when `return` returns a non-Object value (For..of statement)
esid: sec-for-in-and-for-of-statements-runtime-semantics-labelledevaluation
features: [Symbol.iterator, destructuring-binding]
flags: [generated]
info: |
    IterationStatement :
      for ( LeftHandSideExpression of AssignmentExpression ) Statement

    1. Let keyResult be the result of performing ? ForIn/OfHeadEvaluation(« »,
       AssignmentExpression, iterate).
    2. Return ? ForIn/OfBodyEvaluation(LeftHandSideExpression, Statement,
       keyResult, assignment, labelSet).

    13.7.5.13 Runtime Semantics: ForIn/OfBodyEvaluation

    [...]
    4. If destructuring is true and if lhsKind is assignment, then
       a. Assert: lhs is a LeftHandSideExpression.
       b. Let assignmentPattern be the parse of the source text corresponding to
          lhs using AssignmentPattern as the goal symbol.
    [...]

    ArrayAssignmentPattern : [ ]

    1. Let iterator be GetIterator(value).
    2. ReturnIfAbrupt(iterator).
    3. Return IteratorClose(iterator, NormalCompletion(empty)).

    7.4.6 IteratorClose( iterator, completion )

    [...]
    6. Let innerResult be Call(return, iterator, « »).
    7. If completion.[[type]] is throw, return Completion(completion).
    8. If innerResult.[[type]] is throw, return Completion(innerResult).
    9. If Type(innerResult.[[value]]) is not Object, throw a TypeError
       exception.

---*/
var iterable = {};
var iterator = {
  next: function() {
    return { done: true };
  },
  return: function() {
    return null;
  }
};
iterable[Symbol.iterator] = function() {
  return iterator;
};

var counter = 0;

assert.throws(TypeError, function() {
  for ([] of [iterable]) {
    counter += 1;
  }
  counter += 1;
});

assert.sameValue(counter, 0);
