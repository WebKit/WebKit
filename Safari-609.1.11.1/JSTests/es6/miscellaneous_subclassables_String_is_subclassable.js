function test() {

class C extends String {}
var c = new C("golly");
return c instanceof String
  && c + '' === "golly"
  && c[0] === "g"
  && c.length === 5;
      
}

if (!test())
    throw new Error("Test failed");

