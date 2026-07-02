function test() {

var o = {};
var sym = Symbol(), sym2 = Symbol(), sym3 = Symbol();
o[sym]  = true;
o[sym2] = true;
o[sym3] = true;
var result = Object.getOwnPropertySymbols(o);
return result[0] === sym
  && result[1] === sym2
  && result[2] === sym3;
      
}

if (!test())
    throw new Error("Test failed");

