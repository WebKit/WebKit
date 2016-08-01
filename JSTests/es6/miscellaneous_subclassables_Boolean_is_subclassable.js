function test() {

class C extends Boolean {}
var c = new C(true);
return c instanceof Boolean
  && c == true;
      
}

if (!test())
    throw new Error("Test failed");

