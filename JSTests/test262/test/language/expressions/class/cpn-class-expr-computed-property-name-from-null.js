// This file was procedurally generated from the following sources:
// - src/computed-property-names/computed-property-name-from-null.case
// - src/computed-property-names/evaluation/class-expression.template
/*---
description: Computed property name from condition expression (ComputedPropertyName in ClassExpression)
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


let C = class {
  [null]() {
    return null;
  }
  static [null]() {
    return null;
  }
};

let c = new C();

assert.sameValue(
  c[null](),
  null
);
assert.sameValue(
  C[null](),
  null
);
assert.sameValue(
  c[String(null)](),
  null
);
assert.sameValue(
  C[String(null)](),
  null
);
