function test() {

class C extends Array {}
return C.from({ length: 0 }) instanceof C;
      
}

if (!test())
    throw new Error("Test failed");

