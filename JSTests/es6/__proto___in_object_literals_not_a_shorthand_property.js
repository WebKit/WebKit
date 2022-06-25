function test() {

if (!({ __proto__ : [] } instanceof Array)) {
  return false;
}
var __proto__ = [];
return !({ __proto__ } instanceof Array);
      
}

if (!test())
    throw new Error("Test failed");

