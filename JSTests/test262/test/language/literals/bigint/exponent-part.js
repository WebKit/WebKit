// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  It is a Syntax Error if the NumericLiteralBase contains an ExponentPart.
esid: sec-numeric-literal-static-semantics-early-errors
features: [BigInt]
negative:
  phase: parse
  type: SyntaxError
---*/

$DONOTEVALUATE();

0e0n;
