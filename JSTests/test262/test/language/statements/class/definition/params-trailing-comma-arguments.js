// Copyright (C) 2016 Jeff Morrison. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  Check that trailing commas in method parameter lists do not affect `arguments`
  in class method bodies.
info: http://jeffmo.github.io/es-trailing-function-commas/
author: Jeff Morrison <lbljeffmo@gmail.com>
---*/

class C {
  f1() {
    assert.sameValue(
      arguments.length,
      1,
      "Class method called with 1 arg + trailing comma reports " +
      "invalid arguments.length!"
    );
  },

  f2() {
    assert.sameValue(
      arguments.length,
      2,
      "Class method called with 2 arg + trailing comma reports " +
      "invalid arguments.length!"
    );
  }
};

(new C()).f1(1,);
(new C()).f2(1,2,);
