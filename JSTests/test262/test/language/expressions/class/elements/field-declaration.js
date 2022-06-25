// This file was procedurally generated from the following sources:
// - src/class-elements/field-declaration.case
// - src/class-elements/default/cls-expr.template
/*---
description: Fields are defined (field definitions in a class expression)
esid: prod-FieldDefinition
features: [class-fields-public, class]
flags: [generated]
includes: [propertyHelper.js]
info: |
    Updated Productions

    ClassElement :
      ...
      FieldDefinition ;

    FieldDefinition :
      ClassElementName Initializer_opt

    ClassElementName :
      PropertyName

    PropertyName :
      LiteralPropertyName
      ComputedPropertyName

    LiteralPropertyName :
      IdentifierName
      StringLiteral
      NumericLiteral

    ClassDefinitionEvaluation:
      ...

      26. Let instanceFields be a new empty List.
      28. For each ClassElement e in order from elements,
        a. If IsStatic of e is false, then
          i. Let field be the result of performing ClassElementEvaluation for e with arguments proto and false.
        b. ...
        c. ...
        d. If field is not empty, append field to instanceFields.

      ...

      30. Set F.[[Fields]] to instanceFields.
      ...

---*/
var computed = 'h';


var C = class {
  f = 'test262';
  'g';
  0 = 'bar';
  [computed];
}

let c = new C();

assert.sameValue(C.f, undefined);
assert.sameValue(C.g, undefined);
assert.sameValue(C.h, undefined);
assert.sameValue(C[0], undefined);

assert.sameValue(Object.hasOwnProperty.call(C, 'f'), false);
assert.sameValue(Object.hasOwnProperty.call(C, 'g'), false);
assert.sameValue(Object.hasOwnProperty.call(C, 'h'), false);
assert.sameValue(Object.hasOwnProperty.call(C, 0), false);

verifyProperty(c, 'f', {
  value: 'test262',
  enumerable: true,
  writable: true,
  configurable: true
});

verifyProperty(c, 'g', {
  value: undefined,
  enumerable: true,
  writable: true,
  configurable: true
});

verifyProperty(c, 0, {
  value: 'bar',
  enumerable: true,
  writable: true,
  configurable: true
});

verifyProperty(c, 'h', {
  value: undefined,
  enumerable: true,
  writable: true,
  configurable: true
});
