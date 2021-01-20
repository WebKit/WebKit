function test() {

var passed = false;
new function f() {
  passed = (new.target === f);
}();

try {
  Function("new.target = function(){};");
} catch(e) {
  return passed;
}
      
}

if (!test())
    throw new Error("Test failed");

