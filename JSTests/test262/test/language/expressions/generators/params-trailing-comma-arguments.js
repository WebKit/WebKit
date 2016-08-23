// Copyright (C) 2016 Jeff Morrison. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  Check that trailing commas do not affect `arguments` in function
  expression bodies.
info: http://jeffmo.github.io/es-trailing-function-commas/
author: Jeff Morrison <lbljeffmo@gmail.com>
---*/

var f1 = function*() {
  assert.sameValue(
    arguments.length,
    1,
    "Function expression called with 1 arg + trailing comma reports " +
    "invalid arguments.length!"
  );
};
f1(1,).next();

var f2 = function*() {
  assert.sameValue(
    arguments.length,
    2,
    "Function expression called with 2 arg + trailing comma reports " +
    "invalid arguments.length!"
  );
};
f2(1,2,).next();
