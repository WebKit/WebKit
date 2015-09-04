function test() {

return [..."𠮷𠮶"][0] === "𠮷";
      
}

if (!test())
    throw new Error("Test failed");

