function test() {

return /x{1/.exec("x{1")[0] === "x{1"
  && /x]1/.exec("x]1")[0] === "x]1";
      
}

if (!test())
    throw new Error("Test failed");

