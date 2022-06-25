function test() {

class C {
  foo() {}
  static bar() {}
}
return !C.prototype.propertyIsEnumerable("foo") && !C.propertyIsEnumerable("bar");
      
}

if (!test())
    throw new Error("Test failed");

