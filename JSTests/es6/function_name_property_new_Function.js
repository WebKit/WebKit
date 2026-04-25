function test() {

return (new Function).name === "anonymous";
      
}

if (!test())
    throw new Error("Test failed");

