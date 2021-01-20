function test() {

var O = {};
O[Symbol.match] = function(){
  return 42;
};
return ''.match(O) === 42;
      
}

if (!test())
    throw new Error("Test failed");

