function test() {

return (function (a = "baz", b = "qux", c = "quux") {
  a = "corge";
  // The arguments object is not mapped to the
  // parameters, even outside of strict mode.
  return arguments.length === 2
    && arguments[0] === "foo"
    && arguments[1] === "bar";
}("foo", "bar"));
      
}

if (!test())
    throw new Error("Test failed");

