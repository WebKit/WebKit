function test() {

return Array.from({ 0: "foo", 1: "bar", length: 2 }) + '' === "foo,bar";
      
}

if (!test())
    throw new Error("Test failed");

