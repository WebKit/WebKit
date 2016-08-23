// Copyright (C) 2016 Jeff Morrison. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  Check that trailing commas are permitted in function expression
  argument lists.
info: http://jeffmo.github.io/es-trailing-function-commas/
author: Jeff Morrison <lbljeffmo@gmail.com>
---*/
var one = (0, function(a,) {});
var two = (0, function(a,b,) {});
