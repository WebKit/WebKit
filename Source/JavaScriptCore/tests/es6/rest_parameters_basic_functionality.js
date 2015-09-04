function test() {

return (function (foo, ...args) {
  return args instanceof Array && args + "" === "bar,baz";
}("foo", "bar", "baz"));
      
}

if (!test())
    throw new Error("Test failed");

