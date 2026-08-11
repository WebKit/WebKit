var data = [1,2,3];

function fn() {
  var ret = '';
  for (var value of data) {
    ret += value;
  }
  return ret;
}

assertEqual(fn(), '123');

test(fn);
