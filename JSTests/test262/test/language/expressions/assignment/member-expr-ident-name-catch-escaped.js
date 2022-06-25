// This file was procedurally generated from the following sources:
// - src/identifier-names/catch-escaped.case
// - src/identifier-names/default/member-expr.template
/*---
description: catch is a valid identifier name, using escape (MemberExpression IdentifierName)
esid: prod-PropertyDefinition
flags: [generated]
info: |
    MemberExpression:
      ...
      MemberExpression . IdentifierName

    Reserved Words

    A reserved word is an IdentifierName that cannot be used as an Identifier.
---*/

var obj = {};

obj.c\u0061tch = 42;

assert.sameValue(obj['catch'], 42, 'property exists');
