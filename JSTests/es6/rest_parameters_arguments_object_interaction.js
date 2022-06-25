function test() {

return (function (foo, ...args) {
  foo = "qux";
  // The arguments object is not mapped to the
  // parameters, even outside of strict mode.
  return arguments.length === 3
    && arguments[0] === "foo"
    && arguments[1] === "bar"
    && arguments[2] === "baz";
}("foo", "bar", "baz"));
      
}

if (!test())
    throw new Error("Test failed");

