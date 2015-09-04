function test() {

var iterator = (function * generator() {
  yield * [,,];
}());
var item = iterator.next();
var passed = item.value === undefined && item.done === false;
item = iterator.next();
passed    &= item.value === undefined && item.done === false;
item = iterator.next();
passed    &= item.value === undefined && item.done === true;
return passed;
      
}

if (!test())
    throw new Error("Test failed");

