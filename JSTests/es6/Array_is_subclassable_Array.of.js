function test() {

class C extends Array {}
return C.of(0) instanceof C;
      
}

if (!test())
    throw new Error("Test failed");

