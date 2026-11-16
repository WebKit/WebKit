function test() {

var iterable = (function*(){ yield "b"; yield "c"; yield "d"; }());
return ["a", ...iterable, "e"][3] === "d";
      
}

if (!test())
    throw new Error("Test failed");

