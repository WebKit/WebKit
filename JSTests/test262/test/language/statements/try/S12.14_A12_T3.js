// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Using "try" with "catch" or "finally" statement within/without a "for-in"
    statement
es5id: 12.14_A12_T3
description: Try statement inside loop, where use break
---*/

var x;
var mycars = new Array();
mycars[0] = "Saab";
mycars[1] = "Volvo";
mycars[2] = "BMW";

// CHECK#1
var c1=0,fin=0;
for (x in mycars){
  try{
    c1+=1;
    break;
  }
  catch(er1){
    c1+=1;
  }
  finally{
    fin=1;
  }
  fin=-1;
  c1+=2;
};
if(fin!==1){
  throw new Test262Error('#1.1: "finally" block must be evaluated');
}
if(c1!==1){
  throw new Test262Error('#1.2: "try{break}catch finally" must work correctly');
}

// CHECK#2
var c2=0,fin2=0;
for (x in mycars){
  try{
    throw "ex1";
  }
  catch(er1){
    c2+=1;
    break;
  }
  finally{
    fin2=1;
  }
  c2+=2;
  fin2=-1;
}
if(fin2!==1){
  throw new Test262Error('#2.1: "finally" block must be evaluated');
}
if(c2!==1){
  throw new Test262Error('#2.2: "try catch{break} finally" must work correctly');
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
    break;
  }
  c3+=2;
  fin3=0;
}
if(fin3!==1){
  throw new Test262Error('#3.1: "finally" block must be evaluated');
}
if(c3!==1){
  throw new Test262Error('#3.2: "try catch finally{break}" must work correctly');
}

// CHECK#4
var c4=0,fin4=0;
for (x in mycars){
  try{
    c4+=1;
    break;
  }
  finally{
    fin4=1;
  }
  fin4=-1;
  c4+=2;
}
if(fin4!==1){
  throw new Test262Error('#4.1: "finally" block must be evaluated');
}
if(c4!==1){
  throw new Test262Error('#4.2: "try{break} finally" must work correctly');
}

// CHECK#5
var c5=0;
for (x in mycars){
  try{
    throw "ex1";
    c5++;
  }
  catch(er1){
    break;
    c5++;
  }
  c5++;
}
if(c5!==0){
  throw new Test262Error('#5: "try catch{break}" must work correctly');
}

// CHECK#6
var c6=0;
for (x in mycars){
  try{
    c6+=1;
    break;
  }
  catch(er1){}
  c6+=2;
}
if(c6!==1){
  throw new Test262Error('#6: "try{break} catch" must work correctly');
}

// CHECK#7
var c7=0,fin7=0;
try{
  for (x in mycars){
    try{
      c7+=1;
      throw "ex1";
    }
    finally{
      fin7=1;
      break;
    }
    fin7=-1;
    c7+=2;
  }
}
catch(ex1){
  c7=10;
}
if(fin7!==1){
  throw new Test262Error('#7.1: "finally" block must be evaluated');
}
if(c7!==1){
  throw new Test262Error('#7.2: "try finally{break}" must work correctly');
}
