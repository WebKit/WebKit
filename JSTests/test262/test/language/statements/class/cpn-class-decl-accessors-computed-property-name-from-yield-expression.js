// This file was procedurally generated from the following sources:
// - src/computed-property-names/computed-property-name-from-yield-expression.case
// - src/computed-property-names/evaluation/class-declaration-accessors.template
/*---
description: Computed property name from yield expression (ComputedPropertyName in ClassDeclaration)
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
function * g() {


class C {
  get [yield 9]() {
    return 9;
  }

  set [yield 9](v) {
    return 9;
  }

  static get [yield 9]() {
    return 9;
  }

  static set [yield 9](v) {
    return 9;
  }
};

let c = new C();

assert.sameValue(
  c[yield 9],
  9
);
assert.sameValue(
  c[yield 9] = 9,
  9
);

assert.sameValue(
  C[yield 9],
  9
);
assert.sameValue(
  C[yield 9] = 9,
  9
);
assert.sameValue(
  c[String(yield 9)],
  9
);
assert.sameValue(
  c[String(yield 9)] = 9,
  9
);

assert.sameValue(
  C[String(yield 9)],
  9
);
assert.sameValue(
  C[String(yield 9)] = 9,
  9
);

}
var iter = g();
while (iter.next().done === false) ;
