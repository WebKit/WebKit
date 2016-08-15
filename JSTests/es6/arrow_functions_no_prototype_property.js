function test() {

var a = () => 5;
return !a.hasOwnProperty("prototype");
      
}

if (!test())
    throw new Error("Test failed");

