function test() {

new Set();
try {
  Set();
  return false;
} catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

