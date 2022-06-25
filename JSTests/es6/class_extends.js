function test() {

class B {}
class C extends B {}
return new C() instanceof B
  && B.isPrototypeOf(C);
      
}

if (!test())
    throw new Error("Test failed");

