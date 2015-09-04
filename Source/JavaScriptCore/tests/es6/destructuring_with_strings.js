function test() {

var [a, b, c] = "ab";
var d, e;
[d,e] = "de";
return a === "a" && b === "b" && c === undefined
  && d === "d" && e === "e";
      
}

if (!test())
    throw new Error("Test failed");

