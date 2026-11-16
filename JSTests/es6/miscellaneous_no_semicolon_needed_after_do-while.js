function test() {

do {} while (false) return true;
      
}

if (!test())
    throw new Error("Test failed");

