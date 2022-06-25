// This file was procedurally generated from the following sources:
// - src/computed-property-names/computed-property-name-from-condition-expression-true.case
// - src/computed-property-names/evaluation/object-literal.template
/*---
description: Computed property name from condition expression (ComputedPropertyName in ObjectLiteral)
esid: prod-ComputedPropertyName
features: [computed-property-names]
flags: [generated]
info: |
    ObjectLiteral:
      { PropertyDefinitionList }

    PropertyDefinitionList:
      PropertyDefinition

    PropertyDefinition:
      PropertyName: AssignmentExpression

    PropertyName:
      ComputedPropertyName

    ComputedPropertyName:
      [ AssignmentExpression ]
---*/


let o = {
  [true ? 1 : 2]: 2
};

assert.sameValue(
  o[true ? 1 : 2],
  2
);
assert.sameValue(
  o[String(true ? 1 : 2)],
  2
);
