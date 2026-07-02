function test() {

return Object.isExtensible('a') === false;
      
}

if (!test())
    throw new Error("Test failed");

