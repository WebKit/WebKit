function test() {

return "species" in Symbol;
      
}

if (!test())
    throw new Error("Test failed");

