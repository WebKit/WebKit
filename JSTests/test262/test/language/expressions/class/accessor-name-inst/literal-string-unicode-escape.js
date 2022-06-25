// This file was procedurally generated from the following sources:
// - src/accessor-names/literal-string-unicode-escape.case
// - src/accessor-names/default/cls-expr-inst.template
/*---
description: Computed values as accessor property names (string literal containing a Unicode escape sequence) (Class expression, instance method)
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
  get 'unicod\u{000065}Escape'() { return 'get string'; }
  set 'unicod\u{000065}Escape'(param) { stringSet = param; }
};

assert.sameValue(C.prototype['unicodeEscape'], 'get string');

C.prototype['unicodeEscape'] = 'set string';
assert.sameValue(stringSet, 'set string');
