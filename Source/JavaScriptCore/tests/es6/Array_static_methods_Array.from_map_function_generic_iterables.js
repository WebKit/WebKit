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

var iterable = global.__createIterableObject(["foo", "bar", "bal"]);
return Array.from(iterable, function(e, i) {
  return e + this.baz + i;
}, { baz: "d" }) + '' === "food0,bard1,bald2";
      
}

if (!test())
    throw new Error("Test failed");

