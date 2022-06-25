// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The with statement adds a computed object to the front of the
    scope chain of the current execution context
es5id: 12.10_A1.6_T1
description: >
    Using "with" statement within another "with" statement, leading to
    normal completion
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
  with(myObj){
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
  }
}

if(!(p1 === 1)){
  throw new Test262Error('#1: p1 === 1. Actual:  p1 ==='+ p1  );
}

if(!(p2 === "x2")){
  throw new Test262Error('#2: p2 === "x2". Actual:  p2 ==='+ p2  );
}

if(!(p3 === 3)){
  throw new Test262Error('#3: p3 === 3. Actual:  p3 ==='+ p3  );
}

if(!(p4 === "x4")){
  throw new Test262Error('#4: p4 === "x4". Actual:  p4 ==='+ p4  );
}

if(!(p5 === "x5")){
  throw new Test262Error('#5: p5 === "x5". Actual:  p5 ==='+ p5  );
}

if(!(myObj.p1 === "x1")){
  throw new Test262Error('#6: myObj.p1 === "x1". Actual:  myObj.p1 ==='+ myObj.p1  );
}

if(!(myObj.p2 === "b")){
  throw new Test262Error('#7: myObj.p2 === "b". Actual:  myObj.p2 ==='+ myObj.p2  );
}

if(!(myObj.p3 === undefined)){
  throw new Test262Error('#8: myObj.p3 === undefined. Actual:  myObj.p3 ==='+ myObj.p3  );
}

if(!(myObj.p4 === undefined)){
  throw new Test262Error('#9: myObj.p4 === undefined. Actual:  myObj.p4 ==='+ myObj.p4  );
}

if(!(myObj.p5 === undefined)){
  throw new Test262Error('#10: myObj.p5 === undefined. Actual:  myObj.p5 ==='+ myObj.p5  );
}

if(!(st_parseInt !== parseInt)){
  throw new Test262Error('#11: myObj.parseInt !== parseInt');
}

if(!(st_NaN === "obj_NaN")){
  throw new Test262Error('#12: myObj.NaN !== NaN');
}

if(!(st_Infinity !== Infinity)){
  throw new Test262Error('#13: myObj.Infinity !== Infinity');
}

if(!(st_eval !== eval)){
  throw new Test262Error('#14: myObj.eval !== eval');
}

if(!(st_parseFloat !== parseFloat)){
  throw new Test262Error('#15: myObj.parseFloat !== parseFloat');
}

if(!(st_isNaN !== isNaN)){
  throw new Test262Error('#16: myObj.isNaN !== isNaN');
}

if(!(st_isFinite !== isFinite)){
  throw new Test262Error('#17: myObj.isFinite !== isFinite');
}

if(!(value === undefined)){
  throw new Test262Error('#18: value === undefined. Actual:  value ==='+ value  );
}

if(!(myObj.value === "value")){
  throw new Test262Error('#19: myObj.value === "value". Actual:  myObj.value ==='+ myObj.value  );
}
