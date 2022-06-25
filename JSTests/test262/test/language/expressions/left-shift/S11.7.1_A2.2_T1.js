// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator x << y uses [[Default Value]]
es5id: 11.7.1_A2.2_T1
description: If Type(value) is Object, evaluate ToPrimitive(value, Number)
---*/

//CHECK#1
if ({valueOf: function() {return 1}} << 1 !== 2) {
  throw new Test262Error('#1: {valueOf: function() {return 1}} << 1 === 2. Actual: ' + ({valueOf: function() {return 1}} << 1));
}

//CHECK#2
if ({valueOf: function() {return 1}, toString: function() {return 0}} << 1 !== 2) {
  throw new Test262Error('#2: {valueOf: function() {return 1}, toString: function() {return 0}} << 1 === 2. Actual: ' + ({valueOf: function() {return 1}, toString: function() {return 0}} << 1));
}

//CHECK#3
if ({valueOf: function() {return 1}, toString: function() {return {}}} << 1 !== 2) {
  throw new Test262Error('#3: {valueOf: function() {return 1}, toString: function() {return {}}} << 1 === 2. Actual: ' + ({valueOf: function() {return 1}, toString: function() {return {}}} << 1));
}

//CHECK#4
try {
  if ({valueOf: function() {return 1}, toString: function() {throw "error"}} << 1 !== 2) {
    throw new Test262Error('#4.1: {valueOf: function() {return 1}, toString: function() {throw "error"}} << 1 === 2. Actual: ' + ({valueOf: function() {return 1}, toString: function() {throw "error"}} << 1));
  }
}
catch (e) {
  if (e === "error") {
    throw new Test262Error('#4.2: {valueOf: function() {return 1}, toString: function() {throw "error"}} << 1 not throw "error"');
  } else {
    throw new Test262Error('#4.3: {valueOf: function() {return 1}, toString: function() {throw "error"}} << 1 not throw Error. Actual: ' + (e));
  }
}

//CHECK#5
if (1 << {toString: function() {return 1}} !== 2) {
  throw new Test262Error('#5: 1 << {toString: function() {return 1}} === 2. Actual: ' + (1 << {toString: function() {return 1}}));
}

//CHECK#6
if (1 << {valueOf: function() {return {}}, toString: function() {return 1}} !== 2) {
  throw new Test262Error('#6: 1 << {valueOf: function() {return {}}, toString: function() {return 1}} === 2. Actual: ' + (1 << {valueOf: function() {return {}}, toString: function() {return 1}}));
}

//CHECK#7
try {
  1 << {valueOf: function() {throw "error"}, toString: function() {return 1}};
  throw new Test262Error('#7.1: 1 << {valueOf: function() {throw "error"}, toString: function() {return 1}} throw "error". Actual: ' + (1 << {valueOf: function() {throw "error"}, toString: function() {return 1}}));
}  
catch (e) {
  if (e !== "error") {
    throw new Test262Error('#7.2: 1 << {valueOf: function() {throw "error"}, toString: function() {return 1}} throw "error". Actual: ' + (e));
  } 
}

//CHECK#8
try {
  1 << {valueOf: function() {return {}}, toString: function() {return {}}};
  throw new Test262Error('#8.1: 1 << {valueOf: function() {return {}}, toString: function() {return {}}} throw TypeError. Actual: ' + (1 << {valueOf: function() {return {}}, toString: function() {return {}}}));
}  
catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#8.2: 1 << {valueOf: function() {return {}}, toString: function() {return {}}} throw TypeError. Actual: ' + (e));
  } 
}
