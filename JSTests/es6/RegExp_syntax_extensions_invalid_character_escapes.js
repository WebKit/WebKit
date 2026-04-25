function test() {

return /\z/.exec("\\z")[0] === "z"
  && /[\z]/.exec("[\\z]")[0] === "z";
      
}

if (!test())
    throw new Error("Test failed");

