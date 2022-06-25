// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The with statement adds a computed object to the front of the
    scope chain of the current execution context
es5id: 12.10_A1.9_T3
description: >
    Using "for-in" statement within "with" statement, leading to
    completion by break
flags: [noStrict]
---*/

this.p1 = 1;
this.p2 = 2;
this.p3 = 3;
var result = "result";
var myObj = {p1: 'a', 
             p2: 'b', 
             p3: 'c',
             value: 'myObj_value',
             valueOf : function(){return 'obj_valueOf';},
             parseInt : function(){return 'obj_parseInt';},
             NaN : 'obj_NaN',
             Infinity : 'obj_Infinity',
             eval     : function(){return 'obj_eval';},
             parseFloat : function(){return 'obj_parseFloat';},
             isNaN      : function(){return 'obj_isNaN';},
             isFinite   : function(){return 'obj_isFinite';}
}
var del;
var st_p1 = "p1";
var st_p2 = "p2";
var st_p3 = "p3";
var st_parseInt = "parseInt";
var st_NaN = "NaN";
var st_Infinity = "Infinity";
var st_eval = "eval";
var st_parseFloat = "parseFloat";
var st_isNaN = "isNaN";
var st_isFinite = "isFinite";

with(myObj){
  for(var prop in myObj){
    break;
    if(prop === 'p1') {
      st_p1 = p1;
      p1 = 'x1';
    }
    if(prop === 'p2') {
      st_p2 = p2;
      this.p2 = 'x2';
    }
    if(prop === 'p3') {
      st_p3 = p3;
      del = delete p3;
    }
    if(prop === 'parseInt') st_parseInt = parseInt;
    if(prop === 'NaN') st_NaN = NaN;
    if(prop === 'Infinity') st_Infinity = Infinity;
    if(prop === 'eval') st_eval = eval;
    if(prop === 'parseFloat') st_parseFloat = parseFloat;
    if(prop === 'isNaN') st_isNaN = isNaN;
    if(prop === 'isFinite') st_isFinite = isFinite;
    var p4 = 'x4';
    p5 = 'x5';
    var value = 'value';
  }
}

if(!(p1 === 1)){
  throw new Test262Error('#1: p1 === 1. Actual:  p1 ==='+ p1  );
}

if(!(p2 === 2)){
  throw new Test262Error('#2: p2 === 2. Actual:  p2 ==='+ p2  );
}

if(!(p3 === 3)){
  throw new Test262Error('#3: p3 === 3. Actual:  p3 ==='+ p3  );
}

if(!(p4 === undefined)){
  throw new Test262Error('#4: p4 === undefined. Actual:  p4 ==='+ p4  );
}

try {
  p5;
  throw new Test262Error('#5: p5 is not defined');
} catch(e) {    
}

if(!(myObj.p1 === "a")){
  throw new Test262Error('#6: myObj.p1 === "a". Actual:  myObj.p1 ==='+ myObj.p1  );
}

if(!(myObj.p2 === "b")){
  throw new Test262Error('#7: myObj.p2 === "b". Actual:  myObj.p2 ==='+ myObj.p2  );
}

if(!(myObj.p3 === "c")){
  throw new Test262Error('#8: myObj.p3 === "c". Actual:  myObj.p3 ==='+ myObj.p3  );
}

if(!(myObj.p4 === undefined)){
  throw new Test262Error('#9: myObj.p4 === undefined. Actual:  myObj.p4 ==='+ myObj.p4  );
}

if(!(myObj.p5 === undefined)){
  throw new Test262Error('#10: myObj.p5 === undefined. Actual:  myObj.p5 ==='+ myObj.p5  );
}

if(!(st_parseInt === "parseInt")){
  throw new Test262Error('#11: myObj.parseInt === "parseInt". Actual:  myObj.parseInt ==='+ myObj.parseInt  );
}

if(!(st_NaN === "NaN")){
  throw new Test262Error('#12: st_NaN === "NaN". Actual:  st_NaN ==='+ st_NaN  );
}

if(!(st_Infinity === "Infinity")){
  throw new Test262Error('#13: st_Infinity === "Infinity". Actual:  st_Infinity ==='+ st_Infinity  );
}

if(!(st_eval === "eval")){
  throw new Test262Error('#14: st_eval === "eval". Actual:  st_eval ==='+ st_eval  );
}

if(!(st_parseFloat === "parseFloat")){
  throw new Test262Error('#15: st_parseFloat === "parseFloat". Actual:  st_parseFloat ==='+ st_parseFloat  );
}

if(!(st_isNaN === "isNaN")){
  throw new Test262Error('#16: st_isNaN === "isNaN". Actual:  st_isNaN ==='+ st_isNaN  );
}

if(!(st_isFinite === "isFinite")){
  throw new Test262Error('#17: st_isFinite === "isFinite". Actual:  st_isFinite ==='+ st_isFinite  );
}

if(!(value === undefined)){
  throw new Test262Error('#18: value === undefined. Actual:  value ==='+ value  );
}

if(!(myObj.value === "myObj_value")){
  throw new Test262Error('#19: myObj.value === "myObj_value". Actual:  myObj.value ==='+ myObj.value  );
}
