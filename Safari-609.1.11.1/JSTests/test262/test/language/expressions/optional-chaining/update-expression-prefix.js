// Copyright 2019 Google, Inc.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: prod-OptionalExpression
description: >
  optional chaining is forbidden in write contexts
info: |
  UpdateExpression[Yield, Await]:
    LeftHandSideExpression++
    LeftHandSideExpression--
    ++UnaryExpression
    --UnaryExpression
features: [optional-chaining]
negative:
  type: SyntaxError
  phase: parse
---*/

$DONOTEVALUATE();

// --UnaryExpression
const a = {};
--a?.b;
