function test() {

var {c, x:d, e} = {c:7, x:8};
var f, g;
({f,g} = {f:9,g:10});
return c === 7 && d === 8 && e === undefined
  && f === 9 && g === 10;
      
}

if (!test())
    throw new Error("Test failed");

