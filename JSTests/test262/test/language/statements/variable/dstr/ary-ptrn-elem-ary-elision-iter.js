// This file was procedurally generated from the following sources:
// - src/dstr-binding/ary-ptrn-elem-ary-elision-iter.case
// - src/dstr-binding/default/var-stmt.template
/*---
description: BindingElement with array binding pattern and initializer is not used (`var` statement)
esid: sec-variable-statement-runtime-semantics-evaluation
features: [generators, destructuring-binding]
flags: [generated]
info: |
    VariableDeclaration : BindingPattern Initializer

    1. Let rhs be the result of evaluating Initializer.
    2. Let rval be GetValue(rhs).
    3. ReturnIfAbrupt(rval).
    4. Return the result of performing BindingInitialization for
       BindingPattern passing rval and undefined as arguments.

    13.3.3.6 Runtime Semantics: IteratorBindingInitialization

    BindingElement : BindingPatternInitializer opt

    1. If iteratorRecord.[[done]] is false, then
       a. Let next be IteratorStep(iteratorRecord.[[iterator]]).
       [...]
       e. Else,
          i. Let v be IteratorValue(next).
          [...]
    4. Return the result of performing BindingInitialization of BindingPattern
       with v and environment as the arguments.

---*/
var callCount = 0;
function* g() {
  callCount += 1;
};

var [[,] = g()] = [[]];

assert.sameValue(callCount, 0);
