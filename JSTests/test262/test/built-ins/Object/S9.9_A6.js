// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    ToObject conversion from Object: The result is the input
    argument (no conversion)
es5id: 9.9_A6
description: Converting from Objects to Object
---*/

function MyObject(val) {
  this.value = val;
  this.valueOf = function() {
    return this.value;
  }
}

var x = new MyObject(1);
var y = Object(x);

// CHECK#1
if (y.valueOf() !== x.valueOf()) {
  throw new Test262Error('#1: Object(obj).valueOf() === obj.valueOf(). Actual: ' + (Object(obj).valueOf()));
}

// CHECK#2
if (typeof y !== typeof x) {
  throw new Test262Error('#2: typeof Object(obj) === typeof obj. Actual: ' + (typeof Object(obj)));
}

// CHECK#3
if (y.constructor.prototype !== x.constructor.prototype) {
  throw new Test262Error('#3: Object(obj).constructor.prototype === obj.constructor.prototype. Actual: ' + (Object(obj).constructor.prototype));
}


// CHECK#4
if (y !== x) {
  throw new Test262Error('#4: Object(obj) === obj');
}
