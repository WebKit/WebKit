function test() {

return "iterator" in Symbol;
      
}

if (!test())
    throw new Error("Test failed");

