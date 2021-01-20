function test() {

new WeakSet(null);
return true;
      
}

if (!test())
    throw new Error("Test failed");

