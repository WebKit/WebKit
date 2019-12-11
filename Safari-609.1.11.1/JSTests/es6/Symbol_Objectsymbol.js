function test() {

var symbol = Symbol();
var symbolObject = Object(symbol);

return typeof symbolObject === "object" &&
  symbolObject == symbol &&
  symbolObject !== symbol &&
  symbolObject.valueOf() === symbol;
      
}

if (!test())
    throw new Error("Test failed");

