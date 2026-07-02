function test() {

return "𝌆".match(/\u{1d306}/u)[0].length === 2;
      
}

if (!test())
    throw new Error("Test failed");

