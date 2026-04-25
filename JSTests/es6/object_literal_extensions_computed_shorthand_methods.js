function test() {

var x = 'y';
return ({ [x](){ return 1 } }).y() === 1;
      
}

if (!test())
    throw new Error("Test failed");

