// Copyright (C) 2017 Valerie Young. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: static class fields forbid PropName 'prototype' (early error -- PropName of StringLiteral is forbidden)
esid: sec-class-definitions-static-semantics-early-errors
features: [class, class-fields-public]
negative:
  phase: parse
  type: SyntaxError
info: |
    Static Semantics: PropName
    ...
    LiteralPropertyName : StringLiteral
      Return the String value whose code units are the SV of the StringLiteral.


    // This test file tests the following early error:
    Static Semantics: Early Errors

      ClassElement : staticFieldDefinition;
        It is a Syntax Error if PropName of FieldDefinition is "prototype" or "constructor".

---*/


$DONOTEVALUATE();

var C = class {
  static 'prototype';
};
