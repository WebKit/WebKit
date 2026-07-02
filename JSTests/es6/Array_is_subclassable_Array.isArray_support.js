function test() {

class C extends Array {}
return Array.isArray(new C());
      
}

if (!test())
    throw new Error("Test failed");

