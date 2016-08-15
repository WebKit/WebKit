function test() {

class C extends Function {}
var c = new C("this.bar = 2;");
c.prototype.baz = 3;
return new c().bar === 2 && new c().baz === 3;
      
}

if (!test())
    throw new Error("Test failed");

