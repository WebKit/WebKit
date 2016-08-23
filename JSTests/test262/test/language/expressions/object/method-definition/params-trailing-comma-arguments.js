// Copyright (C) 2016 Jeff Morrison. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  Check that trailing parameter commas do not affect `arguments` in object
  method bodies.
info: http://jeffmo.github.io/es-trailing-function-commas/
author: Jeff Morrison <lbljeffmo@gmail.com>
---*/

var obj = {
  f1() {
    assert.sameValue(
      arguments.length,
      1,
      "Object method called with 1 arg + trailing comma reports " +
      "invalid arguments.length!"
    );
  },

  f2() {
    assert.sameValue(
      arguments.length,
      2,
      "Object method called with 2 arg + trailing comma reports " +
      "invalid arguments.length!"
    );
  }
};

obj.f1(1,);
obj.f2(1,2,);
