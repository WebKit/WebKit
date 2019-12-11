function test() {

class C extends Number {}
var c = new C(6);
return c instanceof Number
  && +c === 6;
      
}

if (!test())
    throw new Error("Test failed");

