// This file was procedurally generated from the following sources:
// - src/accessor-names/literal-string-hex-escape.case
// - src/accessor-names/default/cls-decl-static.template
/*---
description: Computed values as accessor property names (string literal containing a hexadecimal escape sequence) (Class declaration, static method)
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
  static get 'hex\x45scape'() { return 'get string'; }
  static set 'hex\x45scape'(param) { stringSet = param; }
}

assert.sameValue(C['hexEscape'], 'get string');

C['hexEscape'] = 'set string';
assert.sameValue(stringSet, 'set string');
