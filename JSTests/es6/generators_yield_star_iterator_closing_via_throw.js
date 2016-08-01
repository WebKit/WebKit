var global = this;

function __createIterableObject(arr, methods) {
  methods = methods || {};
  if (typeof Symbol !== 'function' || !Symbol.iterator) {
    return {};
  }
  arr.length++;
  var iterator = {
    next: function() {
      return { value: arr.shift(), done: arr.length <= 0 };
    },
    'return': methods['return'],
    'throw': methods['throw']
  };
  var iterable = {};
  iterable[Symbol.iterator] = function(){ return iterator; }
  return iterable;
}

function test() {

var closed = false;
var iter = global.__createIterableObject([1, 2, 3], {
  'throw': undefined,
  'return': function() {
    closed = true;
    return {done: true};
  }
});
var gen = (function*(){
  try {
    yield *iter;
  } catch(e){}
})();
gen.next();
gen['throw']();
return closed;
      
}

if (!test())
    throw new Error("Test failed");

