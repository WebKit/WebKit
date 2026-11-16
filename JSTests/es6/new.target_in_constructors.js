function test() {

var passed = false;
new function f() {
  passed = (new.target === f);
}();
(function() {
  passed &= (new.target === undefined);
}());
return passed;
      
}

if (!test())
    throw new Error("Test failed");

