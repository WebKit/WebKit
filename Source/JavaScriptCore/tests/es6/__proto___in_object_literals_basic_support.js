function test() {

return { __proto__ : [] } instanceof Array
  && !({ __proto__ : null } instanceof Object);
      
}

if (!test())
    throw new Error("Test failed");

