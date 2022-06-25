// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-literals-regular-expression-literals-static-semantics-early-errors
info: |
  RegularExpressionFlags ::
    RegularExpressionFlags IdentifierPart

description: >
  It is a Syntax Error if IdentifierPart contains a Unicode escape sequence.
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

/./\u0067;
