function test() {

return /\041/.exec("!")[0] === "!"
  && /[\041]/.exec("!")[0] === "!";
      
}

if (!test())
    throw new Error("Test failed");

