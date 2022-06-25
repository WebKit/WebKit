// This file was procedurally generated from the following sources:
// - src/dstr-binding/ary-ptrn-elem-obj-prop-id-init.case
// - src/dstr-binding/default/func-expr.template
/*---
description: BindingElement with object binding pattern and initializer is used (function expression)
esid: sec-function-definitions-runtime-semantics-evaluation
features: [destructuring-binding]
flags: [generated]
info: |
    FunctionExpression : function ( FormalParameters ) { FunctionBody }

        [...]
        3. Let closure be FunctionCreate(Normal, FormalParameters, FunctionBody,
           scope, strict).
        [...]

    9.2.1 [[Call]] ( thisArgument, argumentsList)

    [...]
    7. Let result be OrdinaryCallEvaluateBody(F, argumentsList).
    [...]

    9.2.1.3 OrdinaryCallEvaluateBody ( F, argumentsList )

    1. Let status be FunctionDeclarationInstantiation(F, argumentsList).
    [...]

    9.2.12 FunctionDeclarationInstantiation(func, argumentsList)

    [...]
    23. Let iteratorRecord be Record {[[iterator]]:
        CreateListIterator(argumentsList), [[done]]: false}.
    24. If hasDuplicates is true, then
        [...]
    25. Else,
        b. Let formalStatus be IteratorBindingInitialization for formals with
           iteratorRecord and env as arguments.
    [...]

    13.3.3.6 Runtime Semantics: IteratorBindingInitialization

    BindingElement : BindingPatternInitializer opt

    [...]
    2. If iteratorRecord.[[done]] is true, let v be undefined.
    3. If Initializer is present and v is undefined, then
       a. Let defaultValue be the result of evaluating Initializer.
       b. Let v be ? GetValue(defaultValue).
    4. Return the result of performing BindingInitialization of BindingPattern
       with v and environment as the arguments.
---*/

var callCount = 0;
var f;
f = function([{ u: v, w: x, y: z } = { u: 444, w: 555, y: 666 }]) {
  assert.sameValue(v, 444);
  assert.sameValue(x, 555);
  assert.sameValue(z, 666);

  assert.throws(ReferenceError, function() {
    u;
  });
  assert.throws(ReferenceError, function() {
    w;
  });
  assert.throws(ReferenceError, function() {
    y;
  });
  callCount = callCount + 1;
};

f([]);
assert.sameValue(callCount, 1, 'function invoked exactly once');
