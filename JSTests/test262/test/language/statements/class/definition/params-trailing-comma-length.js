// Copyright (C) 2016 Jeff Morrison. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  Check that trailing commas don't affect the .length property of
  class methods.
info: http://jeffmo.github.io/es-trailing-function-commas/
author: Jeff Morrison <lbljeffmo@gmail.com>
---*/

class C {
  one(a,) {}
  two(a,b,) {}
};

assert.sameValue(
  C.prototype.one.length,
  1,
  "Class method with 1 arg + trailing comma reports incorrect .length!"
);

assert.sameValue(
  C.prototype.two.length,
  2,
  "Class method with 2 args + trailing comma reports incorrect .length!"
);
