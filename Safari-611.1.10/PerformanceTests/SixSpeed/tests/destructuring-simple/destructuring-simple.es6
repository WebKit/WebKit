var data = {
  a: 'foo',
  b: {c: 'd'},
  arr: [1, 2, 3]
};

function fn() {
  var {a, b} = data;
  return a;
}

assertEqual(fn(), 'foo');

test(fn);
