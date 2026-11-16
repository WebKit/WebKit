function test() {

try {
  eval('for (var i = 0 in {}) {}');
}
catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

