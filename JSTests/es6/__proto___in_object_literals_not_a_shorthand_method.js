function test() {

if (!({ __proto__ : [] } instanceof Array)) {
  return false;
}
return !({ __proto__(){} } instanceof Function);
      
}

if (!test())
    throw new Error("Test failed");

