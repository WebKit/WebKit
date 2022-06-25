// This file was procedurally generated from the following sources:
// - src/identifier-names/switch-escaped.case
// - src/identifier-names/default/obj-prop-name.template
/*---
description: switch is a valid identifier name, using escape (PropertyName)
esid: prod-PropertyDefinition
flags: [generated]
info: |
    ObjectLiteral :
      { PropertyDefinitionList }
      { PropertyDefinitionList , }

    PropertyDefinitionList:
      PropertyDefinition
      PropertyDefinitionList , PropertyDefinition

    PropertyDefinition:
      IdentifierReference
      PropertyName : AssignmentExpression
      MethodDefinition
      ... AssignmentExpression
      ...

    PropertyName:
      LiteralPropertyName
      ...

    LiteralPropertyName:
      IdentifierName
      ...

    Reserved Words

    A reserved word is an IdentifierName that cannot be used as an Identifier.
---*/

var obj = {
  sw\u0069tch: 42
};

assert.sameValue(obj['switch'], 42, 'property exists');
