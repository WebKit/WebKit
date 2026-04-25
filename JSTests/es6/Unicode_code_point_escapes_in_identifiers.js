function test() {

var \u{102C0} = { \u{102C0} : 2 };
return \u{102C0}['\ud800\udec0'] === 2;
      
}

if (!test())
    throw new Error("Test failed");

