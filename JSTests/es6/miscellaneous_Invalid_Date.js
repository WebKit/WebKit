function test() {

return new Date(NaN) + "" === "Invalid Date";
      
}

if (!test())
    throw new Error("Test failed");

