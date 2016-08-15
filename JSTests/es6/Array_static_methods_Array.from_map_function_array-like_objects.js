function test() {

return Array.from({ 0: "foo", 1: "bar", length: 2 }, function(e, i) {
  return e + this.baz + i;
}, { baz: "d" }) + '' === "food0,bard1";
      
}

if (!test())
    throw new Error("Test failed");

