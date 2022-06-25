// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-primary-expression-regular-expression-literals-static-semantics-early-errors
info: |
  PrimaryExpression : RegularExpressionLiteral

description: >
    It is a Syntax Error if BodyText of RegularExpressionLiteral cannot be recognized using the goal symbol Pattern of the ECMAScript RegExp grammar specified in #sec-patterns.
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

/?/;
