function test() {

return /[\w-_]/.exec("-")[0] === "-";
      
}

if (!test())
    throw new Error("Test failed");

