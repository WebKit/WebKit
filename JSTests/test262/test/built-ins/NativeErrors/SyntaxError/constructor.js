// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 19.5.6.1
description: >
  SyntaxError is a constructor function.
---*/

assert.sameValue(typeof SyntaxError, 'function', 'typeof SyntaxError is "function"');
