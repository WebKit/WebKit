function test() {

var passed;
class B {
  constructor() { passed = (new.target === C); }
}
class C extends B {
  constructor() { super(); }
}
new C();
return passed;
      
}

if (!test())
    throw new Error("Test failed");

