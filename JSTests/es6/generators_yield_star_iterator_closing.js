if (typeof global === 'undefined') {
var global = this;
}
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

