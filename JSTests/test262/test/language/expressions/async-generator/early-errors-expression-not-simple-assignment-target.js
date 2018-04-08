// Copyright 2017 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Caitlin Potter <caitp@igalia.com>
esid: prod-LeftHandSideExpression
description: >
  Async generator function expressions are not a simple assignment target.
negative:
  phase: parse
  type: ReferenceError
features: [async-iteration]
---*/

throw "Test262: This statement should not be evaluated.";

(async function*() { } = 1);
