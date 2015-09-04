function test() {

var closed = '';
var iter = __createIterableObject([1, 2, 3], {
  'return': function(){
    closed += 'a';
    return {done: true};
  }
});
var gen = (function* generator(){
  try {
    yield *iter;
  } finally {
    closed += 'b';
  }
})();
gen.next();
gen['return']();
return closed === 'ab';
      
}

if (!test())
    throw new Error("Test failed");

