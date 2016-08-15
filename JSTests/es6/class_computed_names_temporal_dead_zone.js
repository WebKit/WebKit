function test() {

try {
  var B = class C {
    [C](){}
  }
} catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

