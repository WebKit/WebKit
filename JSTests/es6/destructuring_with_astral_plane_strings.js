function test() {

var c;
[c] = "𠮷𠮶";
return c === "𠮷";
      
}

if (!test())
    throw new Error("Test failed");

