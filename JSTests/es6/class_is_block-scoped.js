function test() {

class C {}
var c1 = C;
{
  class C {}
  var c2 = C;
}
return C === c1;
      
}

if (!test())
    throw new Error("Test failed");

