function test() {

new WeakSet();
try {
  WeakSet();
  return false;
} catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

