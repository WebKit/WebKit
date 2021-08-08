// Copyright 2011 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If IsCallable(func) is false, then throw a TypeError exception.
es5id: 15.3.4.5_A16
description: >
    A RegExp is not a function, but it may be callable. Iff it is,
    it's typeof should be 'function', in which case bind should accept
    it as a valid this value.
---*/

var re = (/x/);
if (typeof re === 'function') {
  Function.prototype.bind.call(re, undefined);
} else {
  try {
    Function.prototype.bind.call(re, undefined);
    throw new Test262Error('#1: If IsCallable(func) is false, ' +
      'then (bind should) throw a TypeError exception');
  } catch (e) {
    if (!(e instanceof TypeError)) {
      throw new Test262Error('#1: TypeError expected. Actual: ' + e);
    }
  }
}
