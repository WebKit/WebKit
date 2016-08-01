function test() {

return "𐐘".toLowerCase() === "𐑀" && "𐑀".toUpperCase() === "𐐘";
      
}

if (!test())
    throw new Error("Test failed");

