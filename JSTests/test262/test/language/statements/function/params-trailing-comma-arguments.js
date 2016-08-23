// Copyright (C) 2016 Jeff Morrison. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  Check that trailing commas do not affect `arguments` in function
  declaration bodies.
info: http://jeffmo.github.io/es-trailing-function-commas/
author: Jeff Morrison <lbljeffmo@gmail.com>
---*/

function f1() {
  assert.sameValue(
    arguments.length,
    1,
    "Function declaration called with 1 arg + trailing comma reports " +
    "invalid arguments.length!"
  );
}

function f2() {
  assert.sameValue(
    arguments.length,
    2,
    "Function declaration called with 2 arg + trailing comma reports " +
    "invalid arguments.length!"
  );
}

f1(1,);
f2(1,2,);
