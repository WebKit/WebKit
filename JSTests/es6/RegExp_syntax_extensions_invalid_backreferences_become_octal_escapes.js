function test() {

return /\41/.exec("!")[0] === "!"
  && /[\41]/.exec("!")[0] === "!";
      
}

if (!test())
    throw new Error("Test failed");

