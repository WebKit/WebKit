function test() {

return ["a", ..."bcd", "e"][3] === "d";
      
}

if (!test())
    throw new Error("Test failed");

