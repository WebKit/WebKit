// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: |
    catch(arg) is equivalent to then(undefined, arg)
es6id: S25.4.5.1_A3.1_T2
author: Sam Mikes
description: catch is implemented in terms of then
flags: [async]
---*/

var obj = {};

var p = Promise.reject(obj);

p.then(function() {
  throw new Test262Error("Should not be called: did not expect promise to be fulfilled");
}).catch(function(arg) {
  if (arg !== obj) {
    throw new Test262Error("Should have been rejected with reason obj, got " + arg);
  }
}).then($DONE, $DONE);
