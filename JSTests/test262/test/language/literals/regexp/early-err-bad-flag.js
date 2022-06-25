// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-primary-expression-regular-expression-literals-static-semantics-early-errors
info: |
  PrimaryExpression : RegularExpressionLiteral

description: >
  It is a Syntax Error if FlagText of RegularExpressionLiteral contains any code points other than "g", "i", "m", "u", or "y", ...

negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

/./G;
