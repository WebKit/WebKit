// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: |
    PerformPromiseThen
    Ref 25.4.5.3.1
es6id: S25.4.5.3_A4.2_T2
author: Sam Mikes
description: Promise.prototype.then treats non-callable arg1, arg2 as undefined
flags: [async]
---*/

var obj = {};
var p = Promise.reject(obj);

p.then(3, 5).then(function() {
  throw new Test262Error("Should not be called -- promise was rejected.");
}, function(arg) {
  if (arg !== obj) {
    throw new Test262Error("Expected resolution object to be passed through, got " + arg);
  }
}).then($DONE, $DONE);
