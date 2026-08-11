function test() {

return "𠮷".match(/^.$/u)[0].length === 2;
      
}

if (!test())
    throw new Error("Test failed");

