// Copyright (C) 2016 Jeff Morrison. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  Check that trailing commas are not permitted after rest args in
  class method parameter lists.
info: http://jeffmo.github.io/es-trailing-function-commas/
author: Jeff Morrison <lbljeffmo@gmail.com>
negative: SyntaxError
---*/
class C {
  m(...[],) {}
}
