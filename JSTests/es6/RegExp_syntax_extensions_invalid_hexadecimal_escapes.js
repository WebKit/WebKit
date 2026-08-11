function test() {

return /\x1/.exec("x1")[0] === "x1"
  && /[\x1]/.exec("x")[0] === "x";
      
}

if (!test())
    throw new Error("Test failed");

