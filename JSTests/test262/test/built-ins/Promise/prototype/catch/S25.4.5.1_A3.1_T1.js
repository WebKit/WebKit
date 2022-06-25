// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: |
    catch(arg) is equivalent to then(undefined, arg)
es6id: S25.4.5.1_A3.1_T1
author: Sam Mikes
description: catch is implemented in terms of then
flags: [async]
---*/

var arg = {};

var p = Promise.resolve(arg);

p.catch(function() {
  throw new Test262Error("Should not be called - promise is fulfilled");
}).then(function(result) {
  assert.sameValue(result, arg, 'The value of result is expected to equal the value of arg');
}).then($DONE, $DONE);
