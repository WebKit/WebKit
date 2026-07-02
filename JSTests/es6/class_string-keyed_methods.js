function test() {

class C {
  "foo bar"() { return 2; }
}
return typeof C.prototype["foo bar"] === "function"
  && new C()["foo bar"]() === 2;
      
}

if (!test())
    throw new Error("Test failed");

