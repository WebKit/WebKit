// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The result of evaluating "for(var ExpNoIn;Exp;Exp)" loop is returning
    (normal, evalValue, empty)
es5id: 12.6.3_A9
description: Using eval
---*/

var supreme=5;

var __evaluated =  eval("for(var count=0;;) {if (count===supreme)break;else count++; }");

assert.sameValue(__evaluated, void 0, '#1: __evaluated === 4. Actual:  __evaluated ==='+ __evaluated  );
