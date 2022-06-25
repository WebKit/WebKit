// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-assignment-operators-static-semantics-early-errors
info: |
    It is an early Syntax Error if AssignmentTargetType of
    LeftHandSideExpression is invalid or strict.
description: Logical "??=" assignment with non-simple target
negative:
  phase: parse
  type: SyntaxError
features: [logical-assignment-operators]
---*/
$DONOTEVALUATE();

1 ??= 1;
