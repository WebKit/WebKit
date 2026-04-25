function test() {

--> A comment
<!-- Another comment
var a = 3; <!-- Another comment
return a === 3;
  
}

if (!test())
    throw new Error("Test failed");
