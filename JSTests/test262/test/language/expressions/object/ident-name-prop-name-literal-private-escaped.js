// This file was procedurally generated from the following sources:
// - src/identifier-names/private-escaped.case
// - src/identifier-names/future-reserved-words/obj-prop-name.template
/*---
description: private is a valid identifier name, using escape (PropertyName)
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
  privat\u0065: 42
};

assert.sameValue(obj['private'], 42, 'property exists');
