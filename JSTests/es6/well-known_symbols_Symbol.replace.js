function test() {

var O = {};
O[Symbol.replace] = function(){
  return 42;
};
return ''.replace(O) === 42;
      
}

if (!test())
    throw new Error("Test failed");

