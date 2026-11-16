function test() {

new Proxy({}, {});
try {
  Proxy({}, {});
  return false;
} catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

