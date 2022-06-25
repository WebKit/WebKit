// This file was procedurally generated from the following sources:
// - src/dstr-assignment/array-elem-iter-rtrn-close-null.case
// - src/dstr-assignment/default/assignment-expr.template
/*---
description: IteratorClose throws a TypeError when `return` returns a non-Object value (AssignmentExpression)
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

    ArrayAssignmentPattern : [ AssignmentElementList ]

    [...]
    5. If iteratorRecord.[[done]] is false, return IteratorClose(iterator,
       result).
    6. Return result.

    7.4.6 IteratorClose( iterator, completion )

    [...]
    6. Let innerResult be Call(return, iterator, « »).
    7. If completion.[[type]] is throw, return Completion(completion).
    8. If innerResult.[[type]] is throw, return Completion(innerResult).
    9. If Type(innerResult.[[value]]) is not Object, throw a TypeError
       exception.

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
    return null;
  }
};
var iterable = {};
iterable[Symbol.iterator] = function() {
  return iterator;
};
function* g() {

var result;
var vals = iterable;

result = [ {} = yield ] = vals;

unreachable += 1;

assert.sameValue(result, vals);

}

var iter = g();
iter.next();

assert.sameValue(nextCount, 1);
assert.sameValue(returnCount, 0);
assert.throws(TypeError, function() {
  iter.return();
});
assert.sameValue(nextCount, 1);
assert.sameValue(returnCount, 1);
assert.sameValue(unreachable, 0, 'Unreachable statement was not executed');
