function test() {

class foo {};
class bar { static name() {} };
return foo.name === "foo" &&
  typeof bar.name === "function";
      
}

if (!test())
    throw new Error("Test failed");

