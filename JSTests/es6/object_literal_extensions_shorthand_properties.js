function test() {

var a = 7, b = 8, c = {a,b};
return c.a === 7 && c.b === 8;
      
}

if (!test())
    throw new Error("Test failed");

