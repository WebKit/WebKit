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

var [a, b, c] = global.__createIterableObject([1, 2]);
var d, e;
[d, e] = global.__createIterableObject([3, 4]);
return a === 1 && b === 2 && c === undefined
  && d === 3 && e === 4;
      
}

if (!test())
    throw new Error("Test failed");

