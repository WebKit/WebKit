// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: |
    [...]
    6. Else, let handlerResult be Call(handler, undefined, «argument»).
es6id: S25.4.2.1_A3.1_T1
author: Sam Mikes
description: >
    "fulfilled" handler invoked correctly outside of strict mode
flags: [async, noStrict]
---*/

var expectedThis = this,
  obj = {};

var p = Promise.resolve(obj).then(function(arg) {
  if (this !== expectedThis) {
    $DONE("'this' must be global object, got " + this);
    return;
  }

  if (arg !== obj) {
    $DONE("Expected promise to be fulfilled by obj, actually " + arg);
    return;
  }

  if (arguments.length !== 1) {
    $DONE('Expected handler function to be called with exactly 1 argument.');
    return;
  }

  $DONE();
}, function() {
  $DONE('The promise should not be rejected.');
});
