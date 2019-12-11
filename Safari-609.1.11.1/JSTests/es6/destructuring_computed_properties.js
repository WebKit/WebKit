function test() {

var qux = "corge";
var { [qux]: grault } = { corge: "garply" };
return grault === "garply";
      
}

if (!test())
    throw new Error("Test failed");

