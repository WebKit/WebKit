function test() {

class R extends RegExp {}
var r = new R("baz");
return r.test("foobarbaz");
      
}

if (!test())
    throw new Error("Test failed");

