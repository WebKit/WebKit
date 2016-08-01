function test() {

new Map();
try {
  Map();
  return false;
} catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

