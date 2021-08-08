// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The with statement adds a computed object to the front of the
    scope chain of the current execution context
es5id: 12.10_A1.11_T2
description: >
    Calling a function within "with" statement declared without the
    statement, leading to normal completion by "return"
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

var f = function(){
  st_p1 = p1;
  st_p2 = p2;
  st_p3 = p3;
  st_parseInt = parseInt;
  st_NaN = NaN;
  st_Infinity = Infinity;
  st_eval = eval;
  st_parseFloat = parseFloat;
  st_isNaN = isNaN;
  st_isFinite = isFinite;
  p1 = 'x1';
  this.p2 = 'x2';
  del = delete p3;
  var p4 = 'x4';
  p5 = 'x5';
  var value = 'value';
  return value;
}

with(myObj){
  result = f();
}

if(!(result === "value")){
  throw new Test262Error('#0: result === "value". Actual:  result ==='+ result  );
}

if(!(p1 === "x1")){
  throw new Test262Error('#1: p1 === "x1". Actual:  p1 ==='+ p1  );
}

if(!(p2 === "x2")){
  throw new Test262Error('#2: p2 === "x2". Actual:  p2 ==='+ p2  );
}

try{
  p3;
  throw new Test262Error('#3: p3 is nod defined');
}
catch(e){
}

try {
  p4;
  throw new Test262Error('#4: p4 is not defined');
} catch(e) {    
}

if(!(p5 === "x5")){
  throw new Test262Error('#5: p5 === "x5". Actual:  p5 ==='+ p5  );
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

if(!(st_parseInt === parseInt)){
  throw new Test262Error('#11: st_parseInt === parseInt. Actual:  st_parseInt ==='+ st_parseInt  );
}

assert.sameValue(st_NaN, NaN, "st_NaN is NaN");

if(!(st_Infinity === Infinity)){
  throw new Test262Error('#13: st_Infinity === Infinity. Actual:  st_Infinity ==='+ st_Infinity  );
}

if(!(st_eval === eval)){
  throw new Test262Error('#14: st_eval === eval. Actual:  st_eval ==='+ st_eval  );
}

if(!(st_parseFloat === parseFloat)){
  throw new Test262Error('#15: st_parseFloat === parseFloat. Actual:  st_parseFloat ==='+ st_parseFloat  );
}

if(!(st_isNaN === isNaN)){
  throw new Test262Error('#16: st_isNaN === isNaN. Actual:  st_isNaN ==='+ st_isNaN  );
}

if(!(st_isFinite === isFinite)){
  throw new Test262Error('#17: st_isFinite === isFinite. Actual:  st_isFinite ==='+ st_isFinite  );
}

try {
  value;
  throw new Test262Error('#18: value is not defined');
} catch(e) {    
}

if(!(myObj.value === "myObj_value")){
  throw new Test262Error('#19: myObj.value === "myObj_value". Actual:  myObj.value ==='+ myObj.value  );
}
