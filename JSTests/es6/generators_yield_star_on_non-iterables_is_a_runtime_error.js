function test() {

var iterator = (function * generator() {
  yield * [5];
}());
var item = iterator.next();
var passed = item.value === 5 && item.done === false;
iterator = (function * generator() {
  yield * 5;
}());
try {
  iterator.next();
} catch (e) {
  return passed;
}
      
}

if (!test())
    throw new Error("Test failed");

