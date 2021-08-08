// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    ToObject conversion from Boolean: create a new Boolean object
    whose [[value]] property is set to the value of the boolean
es5id: 9.9_A3
description: Trying to convert from Boolean to Object
---*/

// CHECK#1
if (Object(true).valueOf() !== true) {
  throw new Test262Error('#1: Object(true).valueOf() === true. Actual: ' + (Object(true).valueOf()));
}

// CHECK#2
if (typeof Object(true) !== "object") {
  throw new Test262Error('#2: typeof Object(true) === "object". Actual: ' + (typeof Object(true)));
}

// CHECK#3
if (Object(true).constructor.prototype !== Boolean.prototype) {
  throw new Test262Error('#3: Object(true).constructor.prototype === Boolean.prototype. Actual: ' + (Object(true).constructor.prototype));
}

// CHECK#4
if (Object(false).valueOf() !== false) {
  throw new Test262Error('#4: Object(false).valueOf() === false. Actual: ' + (Object(false).valueOf()));
}

// CHECK#5
if (typeof Object(false) !== "object") {
  throw new Test262Error('#5: typeof Object(false) === "object". Actual: ' + (typeof Object(false)));
}

// CHECK#6
if (Object(false).constructor.prototype !== Boolean.prototype) {
  throw new Test262Error('#6: Object(false).constructor.prototype === Boolean.prototype. Actual: ' + (Object(false).constructor.prototype));
}
