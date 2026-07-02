function test() {

var s = Symbol.toStringTag;
return Math[s] === "Math"
  && JSON[s] === "JSON";
      
}

if (!test())
    throw new Error("Test failed");

