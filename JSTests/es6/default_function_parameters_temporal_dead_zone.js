function test() {

return (function(x = 1) {
  try {
    eval("(function(a=a){}())");
    return false;
  } catch(e) {}
  try {
    eval("(function(a=b,b){}())");
    return false;
  } catch(e) {}
  return true;
}());
      
}

if (!test())
    throw new Error("Test failed");

