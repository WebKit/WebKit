// This file was procedurally generated from the following sources:
// - src/identifier-names/finally-escaped.case
// - src/identifier-names/default/obj-method-definition.template
/*---
description: finally is a valid identifier name, using escape (MethodDefinition)
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
      MethodDefinition
      ...

    MethodDefinition:
      PropertyName ( UniqueFormalParameters ){ FunctionBody }

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
  \u0066inally() { return 42; }
};

assert.sameValue(obj['finally'](), 42, 'property exists');
