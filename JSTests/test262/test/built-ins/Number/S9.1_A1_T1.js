// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Result of primitive conversion from object is a default value for the
    Object
es5id: 9.1_A1_T1
description: >
    Using operator Number. The operator calls ToPrimitive with hint
    Number
---*/

// CHECK#1
var object = {
  valueOf: function() {
    return "1"
  },
  toString: function() {
    return 0
  }
};
if (Number(object) !== 1) {
  throw new Test262Error('#1: var object = {valueOf: function() {return "1"}, toString: function() {return 0}}; Number(object) === 1. Actual: ' + (Number(object)));
}

// CHECK#2
var object = {
  valueOf: function() {
    return {}
  },
  toString: function() {
    return "0"
  }
};
if (Number(object) !== 0) {
  throw new Test262Error('#2: var object = {valueOf: function() {return {}}, toString: function() {return "0"}}; Number(object) === 0. Actual: ' + (Number(object)));
}
