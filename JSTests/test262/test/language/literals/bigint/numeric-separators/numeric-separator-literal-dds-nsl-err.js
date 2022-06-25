// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: prod-NumericLiteralSeparator
description: >
  NumericLiteralSeparator may not be the last digit character of a
  DecimalLiteral
info: |
  NumericLiteral ::
    DecimalIntegerLiteral BigIntLiteralSuffix
    NumericLiteralBase BigIntLiteralSuffix

  NumericLiteralBase ::
    BinaryIntegerLiteral
    OctalIntegerLiteral
    HexIntegerLiteral

  BigIntLiteralSuffix :: n

  NumericLiteralSeparator ::
    _

  DecimalLiteral ::
    DecimalIntegerLiteral . DecimalDigits_opt ExponentPart_opt

  DecimalDigits ::
    ...
    DecimalDigits NumericLiteralSeparator DecimalDigit

negative:
  phase: parse
  type: SyntaxError
features: [BigInt, numeric-separator-literal]
---*/

$DONOTEVALUATE();

10_n;
