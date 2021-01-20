function test() {

return function(a, ...b){}.length === 1 && function(...c){}.length === 0;
      
}

if (!test())
    throw new Error("Test failed");

