function test() {

return Array.isArray(new Proxy([], {}));
      
}

if (!test())
    throw new Error("Test failed");

