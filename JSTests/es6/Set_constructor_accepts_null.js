function test() {

new Set(null);
return true;
      
}

if (!test())
    throw new Error("Test failed");

