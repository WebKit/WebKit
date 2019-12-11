function test() {

var object = {};
var symbol = Symbol();
var value = {};
object[symbol] = value;
return object[symbol] === value;
      
}

if (!test())
    throw new Error("Test failed");

