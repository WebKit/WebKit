// This file was procedurally generated from the following sources:
// - src/accessor-names/literal-numeric-hex.case
// - src/accessor-names/default/cls-expr-inst.template
/*---
description: Computed values as accessor property names (numeric literal in hexadecimal notation) (Class expression, instance method)
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
    3. Return ? ToPropertyKey(propName).
---*/

var stringSet;

var C = class {
  get 0x10() { return 'get string'; }
  set 0x10(param) { stringSet = param; }
};

assert.sameValue(C.prototype['16'], 'get string');

C.prototype['16'] = 'set string';
assert.sameValue(stringSet, 'set string');
