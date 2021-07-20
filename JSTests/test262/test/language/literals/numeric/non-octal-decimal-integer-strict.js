// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-literals-numeric-literals
description: NonOctalDecimalIntegerLiteral is not enabled in strict mode code
info: |
     DecimalIntegerLiteral ::
       0
       NonZeroDigit DecimalDigits[opt]
       NonOctalDecimalIntegerLiteral

     NonOctalDecimalIntegerLiteral ::
       0 NonOctalDigit
       LegacyOctalLikeDecimalIntegerLiteral NonOctalDigit
       NonOctalDecimalIntegerLiteral DecimalDigit

     LegacyOctalLikeDecimalIntegerLiteral ::
       0 OctalDigit
       LegacyOctalLikeDecimalIntegerLiteral OctalDigit

     NonOctalDigit :: one of
       8 9

     ## 12.8.3.1 Static Semantics: Early Errors

     NumericLiteral :: LegacyOctalIntegerLiteral
     DecimalIntegerLiteral :: NonOctalDecimalIntegerLiteral

     - It is a Syntax Error if the source code matching this production is
       strict mode code.
flags: [onlyStrict]
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

08;
