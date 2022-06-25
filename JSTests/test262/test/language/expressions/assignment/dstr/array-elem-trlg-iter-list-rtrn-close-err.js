// This file was procedurally generated from the following sources:
// - src/dstr-assignment/array-elem-trlg-iter-list-rtrn-close-err.case
// - src/dstr-assignment/default/assignment-expr.template
/*---
description: IteratorClose is invoked when evaluation of AssignmentElementList returns a "return" completion and the iterator has not been marked as "done" (AssignmentExpression)
esid: sec-variable-statement-runtime-semantics-evaluation
features: [Symbol.iterator, generators, destructuring-binding]
flags: [generated]
info: |
    VariableDeclaration : BindingPattern Initializer

    1. Let rhs be the result of evaluating Initializer.
    2. Let rval be GetValue(rhs).
    3. ReturnIfAbrupt(rval).
    4. Return the result of performing BindingInitialization for
       BindingPattern passing rval and undefined as arguments.

    ArrayAssignmentPattern :
        [ AssignmentElementList , Elisionopt AssignmentRestElementopt ]

    [...]
    3. Let iteratorRecord be Record {[[iterator]]: iterator, [[done]]: false}.
    4. Let status be the result of performing
       IteratorDestructuringAssignmentEvaluation of AssignmentElementList using
       iteratorRecord as the argument.
    5. If status is an abrupt completion, then
       a. If iteratorRecord.[[done]] is false, return IteratorClose(iterator,
          status).

    7.4.6 IteratorClose( iterator, completion )

    [...]
    6. Let innerResult be Call(return, iterator, « »).
    7. If completion.[[type]] is throw, return Completion(completion).
    8. If innerResult.[[type]] is throw, return Completion(innerResult).

---*/
var nextCount = 0;
var returnCount = 0;
var unreachable = 0;
var iterator = {
  next: function() {
    nextCount += 1;
    return {done: false, value: undefined};
  },
  return: function() {
    returnCount += 1;

    throw new Test262Error();
  }
};
var iterable = {};
iterable[Symbol.iterator] = function() {
  return iterator;
};

function* g() {

var result;
var vals = iterable;

result = [ {} = yield , ] = vals;

unreachable += 1;

assert.sameValue(result, vals);

}

var iter = g();
iter.next();

assert.sameValue(nextCount, 1);
assert.sameValue(returnCount, 0);
assert.throws(Test262Error, function() {
  iter.return();
});
assert.sameValue(nextCount, 1);
assert.sameValue(returnCount, 1);
assert.sameValue(unreachable, 0, 'Unreachable statement was not executed');
