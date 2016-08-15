function test() {

if (!({ __proto__ : [] } instanceof Array)) {
  return false;
}
var a = "__proto__";
return !({ [a] : [] } instanceof Array);
      
}

if (!test())
    throw new Error("Test failed");

