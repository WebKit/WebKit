// Copyright 2015 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: |
 GeneratorMethod early SyntaxError when super is called 
 directly inside generator args
features: [generators]
es6id: 14.4.1
author: Sam Mikes
description: GeneratorMethod error if HasDirectSuper in args
negative:
  phase: parse
  type: SyntaxError
---*/

throw "Test262: This statement should not be evaluated.";

var obj = {
    *foo(a = super()) {
    }
};
