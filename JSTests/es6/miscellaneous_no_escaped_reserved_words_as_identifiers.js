function test() {

var \u0061;
try {
  eval('var v\\u0061r');
} catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

