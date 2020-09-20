// This file was procedurally generated from the following sources:
// - src/computed-property-names/computed-property-name-from-assignment-expression-logical-and.case
// - src/computed-property-names/evaluation/class-declaration-fields.template
/*---
description: Computed property name from assignment expression logical and (ComputedPropertyName in ClassExpression)
esid: prod-ComputedPropertyName
features: [computed-property-names]
flags: [generated]
info: |
    ClassExpression:
      classBindingIdentifier opt ClassTail

    ClassTail:
      ClassHeritage opt { ClassBody opt }

    ClassBody:
      ClassElementList

    ClassElementList:
      ClassElement

    ClassElement:
      MethodDefinition

    MethodDefinition:
      PropertyName ...
      get PropertyName ...
      set PropertyName ...

    PropertyName:
      ComputedPropertyName

    ComputedPropertyName:
      [ AssignmentExpression ]
---*/
let x = 0;


let C = class {
  [x &&= 1] = 2;

  static [x &&= 1] = 2;
};

let c = new C();

assert.sameValue(
  c[x &&= 1],
  2
);
assert.sameValue(
  C[x &&= 1],
  2
);

assert.sameValue(x, 0);
