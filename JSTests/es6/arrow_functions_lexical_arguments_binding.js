function test() {

var f = (function() { return z => arguments[0]; }(5));
return f(6) === 5;
      
}

if (!test())
    throw new Error("Test failed");

