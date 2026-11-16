function test() {

var O = {};
O[Symbol.split] = function(){
  return 42;
};
return ''.split(O) === 42;
      
}

if (!test())
    throw new Error("Test failed");

