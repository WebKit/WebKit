// Copyright (C) 2016 Jeff Morrison. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  Check that trailing commas don't affect the .length property of
  function declarations.
info: http://jeffmo.github.io/es-trailing-function-commas/
author: Jeff Morrison <lbljeffmo@gmail.com>
---*/

function one(a,) {}
function two(a,b,) {}

assert.sameValue(
  one.length,
  1,
  "Function expression with 1 arg + trailing comma reports incorrect .length!"
);

assert.sameValue(
  two.length,
  2,
  "Function expression with 2 args + trailing comma reports incorrect .length!"
);
