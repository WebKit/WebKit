var data = {'a': 'b', 'c': 'd'};
data[Symbol.iterator] = function() {
  var array = Object.keys(data),
      nextIndex = 0;

  return {
    next: function() {
       return nextIndex < array.length ?
         {value: data[array[nextIndex++]], done: false} :
         {done: true};
    }
  };
};

function fn() {
  var ret = '';
  for (var value of data) {
    ret += value;
  }
  return ret;
}

assertEqual(fn(), 'bd');
test(fn);
