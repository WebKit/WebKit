// Copyright (C) 2016 Jeff Morrison. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  Check that trailing commas don't affect the .length property of generator
  function expressions.
info: http://jeffmo.github.io/es-trailing-function-commas/
author: Jeff Morrison <lbljeffmo@gmail.com>
---*/

assert.sameValue(
  (function*(a,) {}).length,
  1,
  "Generator expression with 1 arg + trailing comma reports incorrect .length!"
);

assert.sameValue(
  (function*(a,b,) {}).length,
  2,
  "Generator expression with 2 args + trailing comma reports incorrect .length!"
);
