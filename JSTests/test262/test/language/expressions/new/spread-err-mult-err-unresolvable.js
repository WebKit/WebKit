// This file was procedurally generated from the following sources:
// - src/spread/mult-err-unresolvable.case
// - src/spread/error/member-expr.template
/*---
description: Spread operator following other arguments when reference is unresolvable (`new` operator)
esid: sec-new-operator-runtime-semantics-evaluation
flags: [generated]
info: |
    MemberExpression : new MemberExpression Arguments

    1. Return EvaluateNew(MemberExpression, Arguments).

    12.3.3.1.1 Runtime Semantics: EvaluateNew

    6. If arguments is empty, let argList be an empty List.
    7. Else,
       a. Let argList be ArgumentListEvaluation of arguments.
       [...]

    12.3.6.1 Runtime Semantics: ArgumentListEvaluation

    ArgumentList : ArgumentList , ... AssignmentExpression

    1. Let precedingArgs be the result of evaluating ArgumentList.
    2. Let spreadRef be the result of evaluating AssignmentExpression.
    3. Let iterator be GetIterator(GetValue(spreadRef) ).
    4. ReturnIfAbrupt(iterator).

    6.2.3.1 GetValue (V)

    1. ReturnIfAbrupt(V).
    2. If Type(V) is not Reference, return V.
    3. Let base be GetBase(V).
    4. If IsUnresolvableReference(V), throw a ReferenceError exception.
---*/

assert.throws(ReferenceError, function() {
  new function() {}(0, ...unresolvableReference);
});
