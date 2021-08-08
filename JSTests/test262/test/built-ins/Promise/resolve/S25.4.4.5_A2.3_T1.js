// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
es6id: S25.4.4.5_A2.3_T1
author: Sam Mikes
description: Promise.resolve passes through an unsettled promise w/ same Constructor
flags: [async]
---*/

var rejectP1,
  p1 = new Promise(function(resolve, reject) {
    rejectP1 = reject;
  }),
  p2 = Promise.resolve(p1),
  obj = {};

if (p1 !== p2) {
  throw new Test262Error("Expected p1 === Promise.resolve(p1) because they have same constructor");
}

p2.then(function() {
  throw new Test262Error("Expected p2 to be rejected, not fulfilled.");
}, function(arg) {
  if (arg !== obj) {
    throw new Test262Error("Expected promise to be rejected with reason obj, actually " + arg);
  }
}).then($DONE, $DONE);

rejectP1(obj);
