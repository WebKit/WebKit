// This file was procedurally generated from the following sources:
// - src/accessor-names/computed-err-unresolvable.case
// - src/accessor-names/error/cls-decl-inst.template
/*---
description: Abrupt completion when resolving reference value (Class declaration, instance method)
esid: sec-runtime-semantics-classdefinitionevaluation
features: [class]
flags: [generated]
info: |
    [...]
    21. For each ClassElement m in order from methods
        a. If IsStatic of m is false, then
           i. Let status be the result of performing PropertyDefinitionEvaluation
              for m with arguments proto and false.


    12.2.6.7 Runtime Semantics: Evaluation

    [...]

    ComputedPropertyName : [ AssignmentExpression ]

    1. Let exprValue be the result of evaluating AssignmentExpression.
    2. Let propName be ? GetValue(exprValue).
---*/

assert.throws(ReferenceError, function() {
  class C {
    get [test262unresolvable]() {}
  }
}, '`get` accessor');

assert.throws(ReferenceError, function() {
  class C {
    set [test262unresolvable](_) {}
  }
}, '`set` accessor');
