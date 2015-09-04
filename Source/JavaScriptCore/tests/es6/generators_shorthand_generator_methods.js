function test() {

var o = {
  * generator() {
    yield 5; yield 6;
  },
};
var iterator = o.generator();
var item = iterator.next();
var passed = item.value === 5 && item.done === false;
item = iterator.next();
passed    &= item.value === 6 && item.done === false;
item = iterator.next();
passed    &= item.value === undefined && item.done === true;
return passed;
      
}

if (!test())
    throw new Error("Test failed");

