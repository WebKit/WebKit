// Copyright 2011 Google Inc.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.3.4.5_A5
description: >
    Function.prototype.bind must curry [[Construct]] as  well as
    [[Call]].
---*/

function construct(f, args) {
  var bound = Function.prototype.bind.apply(f, [null].concat(args));
  return new bound();
}
var d = construct(Date, [1957, 4, 27]);
if (Object.prototype.toString.call(d) !== '[object Date]') {
  throw new Test262Error('Using the Date constructor via .bind did not create a Date.');
}
