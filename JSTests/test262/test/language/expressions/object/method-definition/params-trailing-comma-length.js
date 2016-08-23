// Copyright (C) 2016 Jeff Morrison. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  Check that trailing commas don't affect the .length property of
  object methods.
info: http://jeffmo.github.io/es-trailing-function-commas/
author: Jeff Morrison <lbljeffmo@gmail.com>
---*/

var obj = {
  one(a,) {},
  two(a,b,) {},
};

assert.sameValue(
  obj.one.length,
  1,
  "Object method with 1 arg + trailing comma reports incorrect .length!"
);

assert.sameValue(
  obj.two.length,
  2,
  "Object method with 2 args + trailing comma reports incorrect .length!"
);
