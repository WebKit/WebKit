function test() {

class C {}
try {
  C();
}
catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

