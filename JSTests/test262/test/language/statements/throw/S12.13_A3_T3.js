// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: 1. Evaluate Expression
es5id: 12.13_A3_T3
description: Evaluating number expression
---*/

// CHECK#1
try{
  throw 10+3;
}
catch(e){
  if (e!==13) throw new Test262Error('#1: Exception ===13(operaton +). Actual:  Exception ==='+ e);
}

// CHECK#2
var b=10;
var a=3;
try{
  throw a+b;
}
catch(e){
  if (e!==13) throw new Test262Error('#2: Exception ===13(operaton +). Actual:  Exception ==='+ e);
}

// CHECK#3
try{
  throw 3.15-1.02;
}
catch(e){
  if (e!==2.13) throw new Test262Error('#3: Exception ===2.13(operaton -). Actual:  Exception ==='+ e);
}

// CHECK#4
try{
  throw 2*2;
}
catch(e){
  if (e!==4) throw new Test262Error('#4: Exception ===4(operaton *). Actual:  Exception ==='+ e);
}

// CHECK#5
try{
  throw 1+Infinity;
}
catch(e){
  if (e!==+Infinity) throw new Test262Error('#5: Exception ===+Infinity(operaton +). Actual:  Exception ==='+ e);
}

// CHECK#6
try{
  throw 1-Infinity;
}
catch(e){
  if (e!==-Infinity) throw new Test262Error('#6: Exception ===-Infinity(operaton -). Actual:  Exception ==='+ e);
}

// CHECK#7
try{
  throw 10/5;
}
catch(e){
  if (e!==2) throw new Test262Error('#7: Exception ===2(operaton /). Actual:  Exception ==='+ e);
}

// CHECK#8
try{
  throw 8>>2;
}
catch(e){
  if (e!==2) throw new Test262Error('#8: Exception ===2(operaton >>). Actual:  Exception ==='+ e);
}

// CHECK#9
try{
  throw 2<<2;
}
catch(e){
  if (e!==8) throw new Test262Error('#9: Exception ===8(operaton <<). Actual:  Exception ==='+ e);
}

// CHECK#10
try{
  throw 123%100;
}
catch(e){
  if (e!==23) throw new Test262Error('#10: Exception ===23(operaton %). Actual:  Exception ==='+ e);
}
