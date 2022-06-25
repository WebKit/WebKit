// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: prod-NumericLiteral
description: >
  The BigInt suffix is disallowed in LegacyOctalIntegerLiteral
info: |
  NumericLiteral ::
    DecimalIntegerLiteral BigIntLiteralSuffix
    NumericLiteralBase BigIntLiteralSuffix
    LegacyOctalIntegerLiteral

  NumericLiteralBase ::
    BinaryIntegerLiteral
    OctalIntegerLiteral
    HexIntegerLiteral

  BigIntLiteralSuffix :: n
features: [BigInt]
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

01n;
