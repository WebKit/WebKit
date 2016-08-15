function test() {

var sym1 = Symbol("foo");
var sym2 = Symbol();
var o = {
  [sym1]: function(){},
  [sym2]: function(){}
};

return o[sym1].name === "[foo]" &&
       o[sym2].name === "";
      
}

if (!test())
    throw new Error("Test failed");

