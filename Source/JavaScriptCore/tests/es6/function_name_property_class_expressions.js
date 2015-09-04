function test() {

return class foo {}.name === "foo" &&
  typeof class bar { static name() {} }.name === "function";
      
}

if (!test())
    throw new Error("Test failed");

