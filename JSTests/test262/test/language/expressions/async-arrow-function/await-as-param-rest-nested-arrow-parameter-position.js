// Copyright (C) 2018 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-async-arrow-function-definitions
description: >
  It is a SyntaxError if FormalParameters' default expressions contains await.
negative:
  phase: parse
  type: SyntaxError
---*/

throw "Test262: This statement should not be evaluated.";

async(a = (...await) => {}) => {};
