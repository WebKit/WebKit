function test() {

return JSON.stringify(new Proxy(['foo'], {})) === '["foo"]';
      
}

if (!test())
    throw new Error("Test failed");

