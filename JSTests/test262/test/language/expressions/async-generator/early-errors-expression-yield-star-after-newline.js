// Copyright 2017 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Caitlin Potter <caitp@igalia.com>
esid: sec-generator-function-definitions
description: >
  A newline may not precede the `*` token in a `yield` expression.
negative:
  phase: parse
  type: SyntaxError
features: [async-iteration]
---*/

throw "Test262: This statement should not be evaluated.";

(async function*() {
  yield
  * 1;
});
