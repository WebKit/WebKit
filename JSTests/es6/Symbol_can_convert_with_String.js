function test() {

return String(Symbol("foo")) === "Symbol(foo)";
      
}

if (!test())
    throw new Error("Test failed");

