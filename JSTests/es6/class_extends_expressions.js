function test() {

var B;
class C extends (B = class {}) {}
return new C() instanceof B
  && B.isPrototypeOf(C);
      
}

if (!test())
    throw new Error("Test failed");

