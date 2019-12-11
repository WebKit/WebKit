function test() {

new Map(null);
return true;
      
}

if (!test())
    throw new Error("Test failed");

