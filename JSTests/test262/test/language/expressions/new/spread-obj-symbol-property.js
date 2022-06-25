// This file was procedurally generated from the following sources:
// - src/spread/obj-symbol-property.case
// - src/spread/default/member-expr.template
/*---
description: Spread operation where source object contains Symbol properties (`new` operator)
esid: sec-new-operator-runtime-semantics-evaluation
features: [Symbol, object-spread]
flags: [generated]
info: |
    MemberExpression : new MemberExpression Arguments

    1. Return EvaluateNew(MemberExpression, Arguments).

    12.3.3.1.1 Runtime Semantics: EvaluateNew

    6. If arguments is empty, let argList be an empty List.
    7. Else,
       a. Let argList be ArgumentListEvaluation of arguments.
       [...]

    Pending Runtime Semantics: PropertyDefinitionEvaluation

    PropertyDefinition:...AssignmentExpression

    1. Let exprValue be the result of evaluating AssignmentExpression.
    2. Let fromValue be GetValue(exprValue).
    3. ReturnIfAbrupt(fromValue).
    4. Let excludedNames be a new empty List.
    5. Return CopyDataProperties(object, fromValue, excludedNames).

---*/
let symbol = Symbol('foo');
let o = {};
o[symbol] = 1;


var callCount = 0;

new function(obj) {
  assert.sameValue(obj[symbol], 1);
  assert(Object.hasOwnProperty.call(obj, symbol));
  assert.sameValue(obj.c, 4);
  assert.sameValue(obj.d, 5);
  assert.sameValue(Object.keys(obj).length, 2);
  callCount += 1;
}({...o, c: 4, d: 5});

assert.sameValue(callCount, 1);
