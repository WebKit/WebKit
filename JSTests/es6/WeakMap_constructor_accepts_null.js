function test() {

new WeakMap(null);
return true;
      
}

if (!test())
    throw new Error("Test failed");

