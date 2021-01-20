function test() {

var O = {};
O[Symbol.search] = function(){
  return 42;
};
return ''.search(O) === 42;
      
}

if (!test())
    throw new Error("Test failed");

