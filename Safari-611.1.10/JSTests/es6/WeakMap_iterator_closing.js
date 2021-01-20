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
  'return': function(){ closed = true; return {}; }
});
try {
  new WeakMap(iter);
} catch(e){}
return closed;
      
}

if (!test())
    throw new Error("Test failed");

