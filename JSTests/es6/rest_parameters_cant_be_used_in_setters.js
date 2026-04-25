function test() {

return (function (...args) {
  try {
    eval("({set e(...args){}})");
  } catch(e) {
    return true;
  }
}());
      
}

if (!test())
    throw new Error("Test failed");

