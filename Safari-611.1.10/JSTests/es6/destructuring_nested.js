function test() {

var [e, {x:f, g}] = [9, {x:10}];
var {h, x:[i]} = {h:11, x:[12]};
return e === 9 && f === 10 && g === undefined
  && h === 11 && i === 12;
      
}

if (!test())
    throw new Error("Test failed");

