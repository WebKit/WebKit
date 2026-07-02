function test() {

return function({a, b}, [c, d]){}.length === 2;
      
}

if (!test())
    throw new Error("Test failed");

