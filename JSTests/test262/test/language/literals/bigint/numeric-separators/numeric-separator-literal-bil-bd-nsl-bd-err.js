// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: prod-NumericLiteralSeparator
description: >
  NumericLiteralSeparator may not be the appear adjacent to `0b` | `0B` in a
  BinaryIntegerLiteral
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

  BinaryIntegerLiteral ::
    0b BinaryDigits
    0B BinaryDigits

  BinaryDigits ::
    BinaryDigit
    BinaryDigits BinaryDigit
    BinaryDigits NumericLiteralSeparator BinaryDigit

  BinaryDigit :: one of
    0 1

negative:
  phase: parse
  type: SyntaxError
features: [BigInt, numeric-separator-literal]
---*/

$DONOTEVALUATE();

0b_1n;
