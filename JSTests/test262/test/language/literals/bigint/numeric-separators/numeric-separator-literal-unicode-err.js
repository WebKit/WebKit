// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: prod-NumericLiteralSeparator
description: >
  NonZeroDigit NumericLiteralSeparator DecimalDigits sequence expressed with
  unicode escape sequence
info: |
  NumericLiteral ::
    DecimalIntegerLiteral BigIntLiteralSuffix
    NumericLiteralBase BigIntLiteralSuffix

  NumericLiteralBase ::
    BinaryIntegerLiteral
    OctalIntegerLiteral
    HexIntegerLiteral

  BigIntLiteralSuffix :: n

  NumericLiteralSeparator::
    _

  DecimalIntegerLiteral::
    ...
    NonZeroDigit NumericLiteralSeparator_opt DecimalDigits

negative:
  phase: parse
  type: SyntaxError
features: [BigInt, numeric-separator-literal]
---*/

$DONOTEVALUATE();

1\u005F0123456789n;
