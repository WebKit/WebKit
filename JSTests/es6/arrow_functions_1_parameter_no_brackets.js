function test() {

var b = x => x + "foo";
return (b("fee fie foe ") === "fee fie foe foo");
      
}

if (!test())
    throw new Error("Test failed");

