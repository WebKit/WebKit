function test() {

return function ([],{}){
  return arguments[0] + '' === "3,4" && arguments[1].x === "foo";
}([3,4],{x:"foo"});
      
}

if (!test())
    throw new Error("Test failed");

