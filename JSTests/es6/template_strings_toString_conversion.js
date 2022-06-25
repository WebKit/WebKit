function test() {

var a = {
  toString: function() { return "foo"; },
  valueOf: function() { return "bar"; },
};
return `${a}` === "foo";
      
}

if (!test())
    throw new Error("Test failed");

