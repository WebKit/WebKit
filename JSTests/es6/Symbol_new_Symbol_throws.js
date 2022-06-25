function test() {

var symbol = Symbol();
try {
  new Symbol();
} catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

