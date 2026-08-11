function test() {

var a, b, obj = { a:1, b:2 };
return ({a,b} = obj) === obj;
      
}

if (!test())
    throw new Error("Test failed");

