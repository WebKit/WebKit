function test() {

var f = "foo";
return ({f() { return f; }}).f() === "foo";
      
}

if (!test())
    throw new Error("Test failed");

