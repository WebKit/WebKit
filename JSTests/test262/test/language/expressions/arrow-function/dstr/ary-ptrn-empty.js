// This file was procedurally generated from the following sources:
// - src/dstr-binding/ary-ptrn-empty.case
// - src/dstr-binding/default/arrow-function.template
/*---
description: No iteration occurs for an "empty" array binding pattern (arrow function expression)
esid: sec-arrow-function-definitions-runtime-semantics-evaluation
features: [generators, destructuring-binding]
flags: [generated]
info: |
    ArrowFunction : ArrowParameters => ConciseBody

    [...]
    4. Let closure be FunctionCreate(Arrow, parameters, ConciseBody, scope, strict).
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

    ArrayBindingPattern : [ ]

    1. Return NormalCompletion(empty).

---*/
var iterations = 0;
var iter = function*() {
  iterations += 1;
}();

var callCount = 0;
var f;
f = ([]) => {
  assert.sameValue(iterations, 0);
  callCount = callCount + 1;
};

f(iter);
assert.sameValue(callCount, 1, 'arrow function invoked exactly once');
