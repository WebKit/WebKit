// This file was procedurally generated from the following sources:
// - src/class-elements/field-definition-accessor-no-line-terminator.case
// - src/class-elements/default/cls-expr.template
/*---
description: Valid accessor FieldDefinition, ClassElementName, PropertyName Syntax (field definitions in a class expression)
esid: prod-FieldDefinition
features: [decorators, class]
flags: [generated]
info: |
    FieldDefinition[Yield, Await] :
      accessor [no LineTerminator here] ClassElementName[?Yield, ?Await] Initializer[+In, ?Yield, ?Await]opt

---*/


var C = class {
  accessor
  $;
  static accessor
  $;

}

let c = new C();

assert.sameValue(Object.hasOwnProperty.call(C.prototype, 'accessor'), false);
assert.sameValue(Object.hasOwnProperty.call(C.prototype, '$'), false);
assert.sameValue(Object.hasOwnProperty.call(C, 'accessor'), true);
assert.sameValue(Object.hasOwnProperty.call(C, '$'), false);
assert.sameValue(Object.hasOwnProperty.call(c, 'accessor'), true);
assert.sameValue(Object.hasOwnProperty.call(c, '$'), true);
