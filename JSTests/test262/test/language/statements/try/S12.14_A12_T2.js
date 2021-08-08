// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Using "try" with "catch" or "finally" statement within/without a "for-in"
    statement
es5id: 12.14_A12_T2
description: Try statement inside loop, where use continue loop
---*/

var x;
var mycars = new Array();
mycars[0] = "Saab";
mycars[1] = "Volvo";
mycars[2] = "BMW";

// CHECK#1
var fin=0;
var i=0;
for (x in mycars){
  try{
    i+=1;
    continue;
  }
  catch(er1){}
  finally{
    fin=1;
  }
  fin=-1;
}
if(fin!==1){
  throw new Test262Error('#1.1: "finally" block must be evaluated');
}
if(i!==3){
  throw new Test262Error('#1.2:  "try{continue} catch finally" must work correctly');
}

// CHECK#2
var c2=0,fin2=0;
for (x in mycars){
  try{
    throw "ex1";
  }
  catch(er1){
    c2+=1;
    continue;
  }
  finally{
    fin2=1;
  }
  fin2=-1;
}
if(fin2!==1){
  throw new Test262Error('#2.1: "finally" block must be evaluated');
}
if(c2!==3){
  throw new Test262Error('#2.1: "try catch{continue} finally" must work correctly');
}

// CHECK#3
var c3=0,fin3=0;
for (x in mycars){
  try{
    throw "ex1";
  }
  catch(er1){
    c3+=1;
  }
  finally{
    fin3=1;
    continue;
  }
  fin3=0;
}
if(c3!==3){
  throw new Test262Error('#3.1: "finally" block must be evaluated');
}
if(fin3!==1){
  throw new Test262Error('#3.2: "try catch finally{continue}" must work correctly');
}

// CHECK#4
var fin=0;
for (x in mycars){
  try{
    continue;
  }
  finally{
    fin=1;
  }
  fin=-1;
}
if(fin!==1){
  throw new Test262Error('#4: "finally" block must be evaluated at "try{continue} finally" construction');
}

// CHECK#5
var c5=0;
for (x in mycars){
  try{
    throw "ex1";
  }
  catch(er1){
    c5+=1;
    continue;
  }
  c5+=12;
}
if(c5!==3){
  throw new Test262Error('#5: "try catch{continue}" must work correctly');
}

// CHECK#6
var c6=0,fin6=0;
for (x in mycars){
  try{
    c6+=1;
    throw "ex1";
  }
  finally{
    fin6=1;
    continue;
  }
  fin6=-1;
}
if(fin6!==1){
  throw new Test262Error('#6.1: "finally" block must be evaluated');
}
if(c6!==3){
  throw new Test262Error('#6.2: "try finally{continue}" must work correctly');
}
