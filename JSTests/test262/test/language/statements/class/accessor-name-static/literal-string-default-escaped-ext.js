// This file was procedurally generated from the following sources:
// - src/accessor-names/literal-string-default-escaped-ext.case
// - src/accessor-names/default/cls-decl-static.template
/*---
description: Computed values as accessor property names (string literal 'default' w/ extended escape) (Class declaration, static method)
esid: sec-runtime-semantics-classdefinitionevaluation
features: [class]
flags: [generated]
info: |
    [...]
    21. For each ClassElement m in order from methods
        a. If IsStatic of m is false, then
           [...]
        b. Else,
           a. Let status be the result of performing PropertyDefinitionEvaluation
              for m with arguments F and false.


    12.2.6.7 Runtime Semantics: Evaluation

    [...]

    ComputedPropertyName : [ AssignmentExpression ]

    1. Let exprValue be the result of evaluating AssignmentExpression.
    2. Let propName be ? GetValue(exprValue).
    3. Return ? ToPropertyKey(propName).
---*/

var stringSet;

class C {
  static get 'def\u{61}ult'() { return 'get string'; }
  static set 'def\u{61}ult'(param) { stringSet = param; }
}

assert.sameValue(C['default'], 'get string');

C['default'] = 'set string';
assert.sameValue(stringSet, 'set string');
