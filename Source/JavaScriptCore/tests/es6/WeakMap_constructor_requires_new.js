function test() {

new WeakMap();
try {
  WeakMap();
  return false;
} catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

